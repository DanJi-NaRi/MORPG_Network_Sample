#include "MySqlGameplayRepository.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include <mysql.h>

namespace
{
    std::string ReadEnvValue(const char* name)
    {
        if (!name || !(*name))
            return std::string();

        char* buffer = nullptr;
        std::size_t size = 0;
        const errno_t ec = _dupenv_s(&buffer, &size, name);
        if (ec != 0 || !buffer)
            return std::string();

        std::string value(buffer);
        std::free(buffer);
        return value;
    }

    std::string ReadEnvOrDefault(const char* name, const char* fallback)
    {
        const std::string value = ReadEnvValue(name);
        return value.empty() ? std::string(fallback ? fallback : "") : value;
    }

    unsigned int ReadEnvPortOrDefault(const char* name, unsigned int fallback)
    {
        const std::string value = ReadEnvValue(name);
        if (value.empty())
            return fallback;

        const unsigned long parsed = std::strtoul(value.c_str(), nullptr, 10);
        if (parsed == 0 || parsed > 65535UL)
            return fallback;
        return static_cast<unsigned int>(parsed);
    }
}

namespace yuno::server
{
    MySqlGameplayRepository::MySqlGameplayRepository() = default;

    MySqlGameplayRepository::~MySqlGameplayRepository()
    {
        Disconnect();
    }

    bool MySqlGameplayRepository::ConnectFromEnv()
    {
        const std::string host = ReadEnvOrDefault("YUNO_DB_HOST", "127.0.0.1");
        const unsigned int port = ReadEnvPortOrDefault("YUNO_DB_PORT", 3306);
        const std::string user = ReadEnvValue("YUNO_DB_USER");
        const std::string password = ReadEnvValue("YUNO_DB_PASS");
        const std::string database = ReadEnvOrDefault("YUNO_DB_NAME", "yuno_auth");

        if (user.empty())
        {
            m_lastError = "Missing required environment variable: YUNO_DB_USER";
            return false;
        }
        if (password.empty())
        {
            m_lastError = "Missing required environment variable: YUNO_DB_PASS";
            return false;
        }

        return Connect(host.c_str(), port, user.c_str(), password.c_str(), database.c_str());
    }

    void MySqlGameplayRepository::Disconnect()
    {
        if (!m_conn)
            return;
        mysql_close(m_conn);
        m_conn = nullptr;
    }

    bool MySqlGameplayRepository::IsConnected() const
    {
        return m_conn != nullptr;
    }

    bool MySqlGameplayRepository::Connect(
        const char* host,
        unsigned int port,
        const char* user,
        const char* password,
        const char* database)
    {
        Disconnect();

        MYSQL* mysql = mysql_init(nullptr);
        if (!mysql)
        {
            m_lastError = "mysql_init failed.";
            return false;
        }

        if (!mysql_real_connect(mysql, host, user, password, database, port, nullptr, 0))
        {
            m_lastError = mysql_error(mysql);
            mysql_close(mysql);
            return false;
        }

        m_conn = mysql;
        m_lastError.clear();
        return true;
    }

    std::string MySqlGameplayRepository::Escape(const std::string& input)
    {
        if (!m_conn)
            return std::string();

        std::string escaped;
        escaped.resize(input.size() * 2 + 1);
        const unsigned long written = mysql_real_escape_string(
            m_conn,
            escaped.data(),
            input.c_str(),
            static_cast<unsigned long>(input.size()));
        escaped.resize(static_cast<std::size_t>(written));
        return escaped;
    }

    bool MySqlGameplayRepository::Execute(const std::string& sql)
    {
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        if (mysql_query(m_conn, sql.c_str()) != 0)
        {
            m_lastError = mysql_error(m_conn);
            DrainResults();
            return false;
        }

        DrainResults();
        m_lastError.clear();
        return true;
    }

    void MySqlGameplayRepository::DrainResults()
    {
        if (!m_conn)
            return;

        while (true)
        {
            MYSQL_RES* result = mysql_store_result(m_conn);
            if (result)
                mysql_free_result(result);

            const int next = mysql_next_result(m_conn);
            if (next != 0)
                break;
        }
    }

    bool MySqlGameplayRepository::EnsureCharacterForUser(std::uint64_t userId, std::uint32_t& outCharacterId)
    {
        outCharacterId = 0;
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        {
            std::ostringstream oss;
            oss << "SELECT character_id FROM characters WHERE user_id=" << userId << " AND deleted_at IS NULL LIMIT 1";
            if (mysql_query(m_conn, oss.str().c_str()) != 0)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_RES* result = mysql_store_result(m_conn);
            if (!result)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0])
            {
                outCharacterId = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
                mysql_free_result(result);
                DrainResults();
                m_lastError.clear();
                return true;
            }

            mysql_free_result(result);
            DrainResults();
        }

        std::ostringstream nameBuilder;
        nameBuilder << "Demo" << userId;
        std::string characterName = nameBuilder.str();
        if (characterName.size() > 20)
            characterName.resize(20);

        std::ostringstream insertOss;
        insertOss << "INSERT INTO characters(user_id, name, class_code, level, exp, gold) VALUES ("
                  << userId
                  << ", '"
                  << Escape(characterName)
                  << "', 'Blaster', 1, 0, 0)";
        if (!Execute(insertOss.str()))
            return false;

        outCharacterId = static_cast<std::uint32_t>(mysql_insert_id(m_conn));
        return outCharacterId != 0;
    }

    bool MySqlGameplayRepository::LoadInventory(
        std::uint32_t characterId,
        std::vector<PersistedInventoryItem>& outItems,
        std::uint32_t& outGold)
    {
        outItems.clear();
        outGold = 0;
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        {
            std::ostringstream oss;
            oss << "SELECT gold FROM characters WHERE character_id=" << characterId << " LIMIT 1";
            if (mysql_query(m_conn, oss.str().c_str()) != 0)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_RES* result = mysql_store_result(m_conn);
            if (!result)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0])
                outGold = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
            mysql_free_result(result);
            DrainResults();
        }

        {
            std::ostringstream oss;
            oss << "SELECT ii.slot_no, ii.item_id, ii.quantity, COALESCE(i.item_code, '') "
                   "FROM inventory_items ii "
                   "LEFT JOIN items i ON i.item_id = ii.item_id "
                   "WHERE ii.character_id=" << characterId << " ORDER BY ii.slot_no ASC";
            if (mysql_query(m_conn, oss.str().c_str()) != 0)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_RES* result = mysql_store_result(m_conn);
            if (!result)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_ROW row = nullptr;
            while ((row = mysql_fetch_row(result)) != nullptr)
            {
                PersistedInventoryItem item{};
                item.slotNo = static_cast<std::uint16_t>(row[0] ? std::strtoul(row[0], nullptr, 10) : 0);
                item.itemId = static_cast<std::uint32_t>(row[1] ? std::strtoul(row[1], nullptr, 10) : 0);
                item.quantity = static_cast<std::uint16_t>(row[2] ? std::strtoul(row[2], nullptr, 10) : 0);
                item.itemCode = row[3] ? row[3] : "";
                outItems.push_back(std::move(item));
            }
            mysql_free_result(result);
            DrainResults();
        }

        m_lastError.clear();
        return true;
    }

    bool MySqlGameplayRepository::EnsureRewardItem(std::uint32_t& outItemId, std::string& outItemCode)
    {
        outItemId = 0;
        outItemCode = "DEMO_DUNGEON_TOKEN";
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        {
            std::ostringstream oss;
            oss << "SELECT item_id FROM items WHERE item_code='" << outItemCode << "' LIMIT 1";
            if (mysql_query(m_conn, oss.str().c_str()) != 0)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_RES* result = mysql_store_result(m_conn);
            if (!result)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0])
            {
                outItemId = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
                mysql_free_result(result);
                DrainResults();
                return true;
            }
            mysql_free_result(result);
            DrainResults();
        }

        std::ostringstream insertOss;
        insertOss << "INSERT INTO items(item_code, item_name, item_type, rarity, max_stack, sell_price, is_tradeable, is_active) VALUES ("
                  << "'" << outItemCode << "', "
                  << "'Demo Dungeon Token', 'quest', 'rare', 999, 0, 0, 1)";
        if (!Execute(insertOss.str()))
            return false;

        outItemId = static_cast<std::uint32_t>(mysql_insert_id(m_conn));
        return outItemId != 0;
    }

    bool MySqlGameplayRepository::GrantDemoDungeonReward(std::uint32_t characterId, RewardGrantResult& outResult)
    {
        outResult = RewardGrantResult{};
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        std::uint32_t rewardItemId = 0;
        std::string rewardItemCode;
        if (!EnsureRewardItem(rewardItemId, rewardItemCode))
            return false;

        outResult.rewardItemId = rewardItemId;
        outResult.rewardItemCode = rewardItemCode;
        outResult.rewardQuantity = 1;
        outResult.goldAward = 25;

        if (!Execute("START TRANSACTION"))
            return false;

        bool ok = true;
        std::uint32_t inventoryId = 0;
        std::uint32_t currentQuantity = 0;
        {
            std::ostringstream oss;
            oss << "SELECT inventory_id, quantity FROM inventory_items WHERE character_id=" << characterId
                << " AND item_id=" << rewardItemId << " ORDER BY inventory_id ASC LIMIT 1";
            if (mysql_query(m_conn, oss.str().c_str()) != 0)
            {
                m_lastError = mysql_error(m_conn);
                DrainResults();
                ok = false;
            }
            else
            {
                MYSQL_RES* result = mysql_store_result(m_conn);
                if (!result)
                {
                    m_lastError = mysql_error(m_conn);
                    DrainResults();
                    ok = false;
                }
                else
                {
                    MYSQL_ROW row = mysql_fetch_row(result);
                    if (row && row[0])
                    {
                        inventoryId = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
                        currentQuantity = static_cast<std::uint32_t>(row[1] ? std::strtoul(row[1], nullptr, 10) : 0);
                    }
                    mysql_free_result(result);
                    DrainResults();
                }
            }
        }

        if (ok)
        {
            if (inventoryId != 0)
            {
                std::ostringstream updateOss;
                updateOss << "UPDATE inventory_items SET quantity=" << (currentQuantity + outResult.rewardQuantity)
                          << " WHERE inventory_id=" << inventoryId;
                ok = Execute(updateOss.str());
            }
            else
            {
                std::uint32_t nextSlot = 0;
                std::ostringstream slotOss;
                slotOss << "SELECT COALESCE(MAX(slot_no), -1) + 1 FROM inventory_items WHERE character_id=" << characterId;
                if (mysql_query(m_conn, slotOss.str().c_str()) != 0)
                {
                    m_lastError = mysql_error(m_conn);
                    DrainResults();
                    ok = false;
                }
                else
                {
                    MYSQL_RES* result = mysql_store_result(m_conn);
                    if (!result)
                    {
                        m_lastError = mysql_error(m_conn);
                        DrainResults();
                        ok = false;
                    }
                    else
                    {
                        MYSQL_ROW row = mysql_fetch_row(result);
                        if (row && row[0])
                            nextSlot = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
                        mysql_free_result(result);
                        DrainResults();
                    }
                }

                if (ok)
                {
                    std::ostringstream insertOss;
                    insertOss << "INSERT INTO inventory_items(character_id, slot_no, item_id, quantity, is_bound) VALUES ("
                              << characterId << ", " << nextSlot << ", " << rewardItemId << ", "
                              << outResult.rewardQuantity << ", 1)";
                    ok = Execute(insertOss.str());
                }
            }
        }

        if (ok)
        {
            std::ostringstream goldOss;
            goldOss << "UPDATE characters SET gold = gold + " << outResult.goldAward << " WHERE character_id=" << characterId;
            ok = Execute(goldOss.str());
        }

        if (!ok)
        {
            Execute("ROLLBACK");
            return false;
        }

        if (!Execute("COMMIT"))
            return false;

        return LoadInventory(characterId, outResult.items, outResult.totalGold);
    }
}
