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

    bool MySqlAuthRepository::ValidateUserCredentials(
        const std::string& username,
        const std::string& password,
        std::uint64_t& outUserId)
    {
        outUserId = 0;

        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        const std::string escapedUsername = Escape(username);
        const std::string escapedPw = Escape(password);

        std::ostringstream oss;
        oss << "SELECT user_id, (password_hash='"
            << escapedPw
            << "') AS legacy_plaintext_match "
            << "FROM users WHERE username='"
            << escapedUsername
            << "' AND status=1 "
            << "AND (password_hash=SHA2('"
            << escapedPw
            << "', 256) OR password_hash='"
            << escapedPw
            << "') LIMIT 1";

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

        outUserId = static_cast<std::uint64_t>(std::strtoull(row[0], nullptr, 10));
        const bool legacyPlaintextMatch = (row[1] != nullptr && std::strtoul(row[1], nullptr, 10) != 0UL);
        mysql_free_result(result);

        if (legacyPlaintextMatch)
        {
            std::ostringstream migrateOss;
            migrateOss << "UPDATE users SET password_hash=SHA2('"
                       << escapedPw
                       << "', 256) WHERE user_id="
                       << outUserId;

            (void)Execute(migrateOss.str());
        }

        m_lastError.clear();
        return true;
    }

    bool MySqlAuthRepository::CreateUser(const std::string& username, const std::string& password, bool& outAlreadyExists)
    {
        outAlreadyExists = false;

        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        const std::string escapedUsername = Escape(username);
        const std::string escapedPw = Escape(password);

        std::ostringstream oss;
        oss << "INSERT INTO users(username, password_hash, status, created_at) VALUES ('"
            << escapedUsername
            << "', SHA2('"
            << escapedPw
            << "', 256), 1, NOW())";

        if (mysql_query(m_conn, oss.str().c_str()) != 0)
        {
            const unsigned int errNo = mysql_errno(m_conn);
            if (errNo == 1062U)
            {
                outAlreadyExists = true;
                m_lastError = "Account already exists.";
                return false;
            }

            m_lastError = mysql_error(m_conn);
            return false;
        }

        m_lastError.clear();
        return true;
    }

    bool MySqlAuthRepository::UpsertLoginToken(
        std::uint64_t userId,
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
        oss << "INSERT INTO login_tokens(user_id, token_hash, ip_address, created_at, expires_at) VALUES ("
            << userId
            << ", SHA2('"
            << escapedToken
            << "', 256), NULL, NOW(), FROM_UNIXTIME("
            << outExpiresAtEpoch
            << ")) "
            << "ON DUPLICATE KEY UPDATE token_hash=VALUES(token_hash), ip_address=VALUES(ip_address), "
            << "expires_at=VALUES(expires_at), created_at=VALUES(created_at), revoked_at=NULL";

        return Execute(oss.str());
    }

    bool MySqlAuthRepository::TouchLastLogin(std::uint64_t userId)
    {
        std::ostringstream oss;
        oss << "UPDATE users SET last_login_at=NOW() WHERE user_id=" << userId;
        return Execute(oss.str());
    }

    bool MySqlAuthRepository::HasActiveLoginToken(std::uint64_t userId, bool& outHasActiveToken)
    {
        outHasActiveToken = false;

        if (!m_conn)
        {
            m_lastError = "DB is not connected.";
            return false;
        }

        std::ostringstream oss;
        oss << "SELECT token_id FROM login_tokens "
            << "WHERE user_id=" << userId
            << " AND revoked_at IS NULL "
            << "AND expires_at > NOW() "
            << "LIMIT 1";

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
        outHasActiveToken = (row != nullptr && row[0] != nullptr);
        mysql_free_result(result);

        m_lastError.clear();
        return true;
    }

    bool MySqlAuthRepository::RevokeLoginToken(std::uint64_t userId)
    {
        std::ostringstream oss;
        oss << "UPDATE login_tokens "
            << "SET revoked_at=NOW() "
            << "WHERE user_id=" << userId << " AND revoked_at IS NULL";
        return Execute(oss.str());
    }

    bool MySqlAuthRepository::RevokeLoginTokenByHash(const std::string& tokenHash)
    {
        const std::string escapedToken = Escape(tokenHash);

        std::ostringstream oss;
        oss << "UPDATE login_tokens "
            << "SET revoked_at=NOW() "
            << "WHERE (token_hash=SHA2('" << escapedToken << "', 256) OR token_hash='" << escapedToken << "') "
            << "AND revoked_at IS NULL";

        return Execute(oss.str());
    }
}
