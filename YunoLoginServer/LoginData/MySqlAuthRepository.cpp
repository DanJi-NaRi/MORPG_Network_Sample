#include "MySqlAuthRepository.h"

#include <chrono>
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
        const std::string v = ReadEnvValue(name);
        if (v.empty())
            return fallback ? std::string(fallback) : std::string();

        return v;
    }

    unsigned int ReadEnvPortOrDefault(const char* name, unsigned int fallback)
    {
        const std::string v = ReadEnvValue(name);
        if (v.empty())
            return fallback;

        const unsigned long parsed = std::strtoul(v.c_str(), nullptr, 10);
        if (parsed == 0 || parsed > 65535UL)
            return fallback;

        return static_cast<unsigned int>(parsed);
    }
}

namespace yuno::login
{
    MySqlAuthRepository::MySqlAuthRepository() = default;

    MySqlAuthRepository::~MySqlAuthRepository()
    {
        Disconnect();
    }

    bool MySqlAuthRepository::ConnectFromEnv()
    {
        const std::string host = ReadEnvOrDefault("YUNO_DB_HOST", "127.0.0.1");
        const unsigned int port = ReadEnvPortOrDefault("YUNO_DB_PORT", 3306);
        const std::string user = ReadEnvOrDefault("YUNO_DB_USER", "***REMOVED***");
        const std::string password = ReadEnvOrDefault("YUNO_DB_PASS", "***REMOVED***");
        const std::string database = ReadEnvOrDefault("YUNO_DB_NAME", "yuno_auth");

        return Connect(host.c_str(), port, user.c_str(), password.c_str(), database.c_str());
    }

    void MySqlAuthRepository::Disconnect()
    {
        if (!m_conn)
            return;

        mysql_close(m_conn);
        m_conn = nullptr;
    }

    bool MySqlAuthRepository::IsConnected() const
    {
        return m_conn != nullptr;
    }

    bool MySqlAuthRepository::Connect(
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

    std::string MySqlAuthRepository::Escape(const std::string& input)
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

    bool MySqlAuthRepository::Execute(const std::string& sql)
    {
        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        if (mysql_query(m_conn, sql.c_str()) != 0)
        {
            m_lastError = mysql_error(m_conn);
            return false;
        }

        m_lastError.clear();
        return true;
    }

    bool MySqlAuthRepository::ValidateAccount(
        const std::string& accountId,
        const std::string& password,
        std::uint64_t& outAccountDbId)
    {
        outAccountDbId = 0;

        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        const std::string escapedAccount = Escape(accountId);
        const std::string escapedPw = Escape(password);

        std::ostringstream oss;
        oss << "SELECT id FROM accounts WHERE account_id='"
            << escapedAccount
            << "' AND password='"
            << escapedPw
            << "' LIMIT 1";

        if (mysql_query(m_conn, oss.str().c_str()) != 0)
        {
            m_lastError = mysql_error(m_conn);
            return false;
        }

        MYSQL_RES* result = mysql_store_result(m_conn);
        if (!result)
        {
            m_lastError = mysql_error(m_conn);
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        if (!row || !row[0])
        {
            mysql_free_result(result);
            m_lastError = "Invalid account or password.";
            return false;
        }

        outAccountDbId = static_cast<std::uint64_t>(std::strtoull(row[0], nullptr, 10));
        mysql_free_result(result);
        m_lastError.clear();
        return true;
    }

    bool MySqlAuthRepository::UpsertLoginToken(
        std::uint64_t accountDbId,
        const std::string& token,
        std::uint32_t ttlSeconds,
        std::uint64_t& outExpiresAtEpoch)
    {
        outExpiresAtEpoch = 0;

        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        const std::uint64_t nowEpoch = static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        outExpiresAtEpoch = nowEpoch + static_cast<std::uint64_t>(ttlSeconds);

        const std::string escapedToken = Escape(token);

        std::ostringstream oss;
        oss << "INSERT INTO login_tokens(account_id, token, expires_at, created_at) VALUES ("
            << accountDbId
            << ", '"
            << escapedToken
            << "', FROM_UNIXTIME("
            << outExpiresAtEpoch
            << "), NOW()) "
            << "ON DUPLICATE KEY UPDATE account_id=VALUES(account_id), expires_at=VALUES(expires_at), created_at=VALUES(created_at)";

        return Execute(oss.str());
    }
}
