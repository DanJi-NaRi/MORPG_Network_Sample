#include "MySqlAuthRepository.h"

#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#include <mysql.h>
#include <argon2.h>

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

    constexpr std::uint32_t kArgon2TimeCost = 2;
    constexpr std::uint32_t kArgon2MemoryCostKiB = 32 * 1024;
    constexpr std::uint32_t kArgon2Parallelism = 1;
    constexpr std::size_t kArgon2SaltLength = 16;
    constexpr std::size_t kArgon2HashLength = 32;

    bool FillSecureRandom(std::uint8_t* outBytes, std::size_t len)
    {
        if (!outBytes || len == 0)
            return false;

        std::random_device rd;
        for (std::size_t i = 0; i < len; ++i)
        {
            outBytes[i] = static_cast<std::uint8_t>(rd());
        }

        return true;
    }

    bool IsArgon2idHash(const std::string& stored)
    {
        return stored.rfind("$argon2id$", 0) == 0;
    }

    bool HashPasswordArgon2id(const std::string& password, std::string& outEncodedHash)
    {
        std::vector<std::uint8_t> salt(kArgon2SaltLength);
        if (!FillSecureRandom(salt.data(), salt.size()))
            return false;

        const std::size_t encodedLen = argon2_encodedlen(
            kArgon2TimeCost,
            kArgon2MemoryCostKiB,
            kArgon2Parallelism,
            static_cast<std::uint32_t>(kArgon2SaltLength),
            static_cast<std::uint32_t>(kArgon2HashLength),
            Argon2_id);

        if (encodedLen == 0 || encodedLen > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            return false;

        std::vector<char> encoded(encodedLen, '\0');
        const int rc = argon2id_hash_encoded(
            kArgon2TimeCost,
            kArgon2MemoryCostKiB,
            kArgon2Parallelism,
            password.data(),
            password.size(),
            salt.data(),
            salt.size(),
            kArgon2HashLength,
            encoded.data(),
            encoded.size());

        if (rc != ARGON2_OK)
            return false;

        outEncodedHash = encoded.data();
        return !outEncodedHash.empty();
    }

    bool VerifyPasswordArgon2id(const std::string& encodedHash, const std::string& password)
    {
        const int rc = argon2id_verify(encodedHash.c_str(), password.data(), password.size());
        return rc == ARGON2_OK;
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
        oss << "SELECT user_id, password_hash, "
            << "(password_hash=SHA2('" << escapedPw << "', 256)) AS legacy_sha256_match, "
            << "(password_hash='" << escapedPw << "') AS legacy_plaintext_match "
            << "FROM users WHERE username='"
            << escapedUsername
            << "' AND status=1 LIMIT 1";

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
        if (!row || !row[0] || !row[1])
        {
            mysql_free_result(result);
            m_lastError = "Invalid account or password.";
            return false;
        }

        const std::uint64_t matchedUserId = static_cast<std::uint64_t>(std::strtoull(row[0], nullptr, 10));
        const std::string storedHash = row[1];
        const bool legacySha256Match = (row[2] != nullptr && std::strtoul(row[2], nullptr, 10) != 0UL);
        const bool legacyPlaintextMatch = (row[3] != nullptr && std::strtoul(row[3], nullptr, 10) != 0UL);
        mysql_free_result(result);

        bool authenticated = false;
        bool needsMigration = false;

        if (IsArgon2idHash(storedHash))
        {
            authenticated = VerifyPasswordArgon2id(storedHash, password);
        }
        else
        {
            authenticated = legacySha256Match || legacyPlaintextMatch;
            needsMigration = authenticated;
        }

        if (!authenticated)
        {
            m_lastError = "Invalid account or password.";
            return false;
        }

        outUserId = matchedUserId;

        if (needsMigration)
        {
            std::string encodedHash;
            if (!HashPasswordArgon2id(password, encodedHash))
            {
                m_lastError = "Failed to generate Argon2id hash.";
                return false;
            }

            const std::string escapedArgonHash = Escape(encodedHash);
            std::ostringstream migrateOss;
            migrateOss << "UPDATE users SET password_hash='"
                       << escapedArgonHash
                       << "' WHERE user_id=" << outUserId;

            if (!Execute(migrateOss.str()))
                return false;
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

        std::string encodedHash;
        if (!HashPasswordArgon2id(password, encodedHash))
        {
            m_lastError = "Failed to generate Argon2id hash.";
            return false;
        }
        const std::string escapedHash = Escape(encodedHash);

        std::ostringstream oss;
        oss << "INSERT INTO users(username, password_hash, status, created_at) VALUES ('"
            << escapedUsername
            << "', '"
            << escapedHash
            << "', 1, NOW())";

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
