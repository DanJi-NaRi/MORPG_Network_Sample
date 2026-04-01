#pragma once

#include <cstdint>
#include <string>

struct MYSQL;

namespace yuno::login
{
    class MySqlAuthRepository final
    {
    public:
        MySqlAuthRepository();
        ~MySqlAuthRepository();

        MySqlAuthRepository(const MySqlAuthRepository&) = delete;
        MySqlAuthRepository& operator=(const MySqlAuthRepository&) = delete;

        bool ConnectFromEnv();
        void Disconnect();
        bool IsConnected() const;

        bool ValidateUserCredentials(const std::string& username, const std::string& password, std::uint64_t& outUserId);
        bool CreateUser(const std::string& username, const std::string& password, bool& outAlreadyExists);
        bool UpsertLoginToken(std::uint64_t userId, const std::string& token, std::uint32_t ttlSeconds, std::uint64_t& outExpiresAtEpoch);
        bool TouchLastLogin(std::uint64_t userId);
        bool HasActiveLoginToken(std::uint64_t userId, bool& outHasActiveToken);
        bool RevokeLoginToken(std::uint64_t userId);
        bool RevokeLoginTokenByHash(const std::string& tokenHash);

        const std::string& LastError() const { return m_lastError; }

    private:
        bool Connect(
            const char* host,
            unsigned int port,
            const char* user,
            const char* password,
            const char* database);

        std::string Escape(const std::string& input);
        bool Execute(const std::string& sql);

    private:
        MYSQL* m_conn = nullptr;
        std::string m_lastError;
    };
}
