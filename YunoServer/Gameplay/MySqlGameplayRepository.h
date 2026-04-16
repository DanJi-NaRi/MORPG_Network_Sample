#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MYSQL;

namespace yuno::server
{
    struct PersistedInventoryItem final
    {
        std::uint16_t slotNo = 0;
        std::uint32_t itemId = 0;
        std::uint16_t quantity = 0;
        std::string itemCode;
    };

    struct RewardGrantResult final
    {
        std::uint32_t rewardItemId = 0;
        std::string rewardItemCode;
        std::uint16_t rewardQuantity = 0;
        std::uint32_t goldAward = 0;
        std::uint32_t totalGold = 0;
        std::vector<PersistedInventoryItem> items;
    };

    class MySqlGameplayRepository final
    {
    public:
        MySqlGameplayRepository();
        ~MySqlGameplayRepository();

        MySqlGameplayRepository(const MySqlGameplayRepository&) = delete;
        MySqlGameplayRepository& operator=(const MySqlGameplayRepository&) = delete;

        bool ConnectFromEnv();
        void Disconnect();
        bool IsConnected() const;

        bool EnsureCharacterForUser(std::uint64_t userId, std::uint32_t& outCharacterId);
        bool LoadCharacterName(std::uint32_t characterId, std::string& outName);
        bool LoadInventory(std::uint32_t characterId, std::vector<PersistedInventoryItem>& outItems, std::uint32_t& outGold);
        bool GrantDemoDungeonReward(std::uint32_t characterId, RewardGrantResult& outResult);

        const std::string& LastError() const { return m_lastError; }

    private:
        bool Connect(const char* host, unsigned int port, const char* user, const char* password, const char* database);
        std::string Escape(const std::string& input);
        bool Execute(const std::string& sql);
        void DrainResults();
        bool EnsureRewardItem(std::uint32_t& outItemId, std::string& outItemCode);

    private:
        MYSQL* m_conn = nullptr;
        std::string m_lastError;
    };
}
