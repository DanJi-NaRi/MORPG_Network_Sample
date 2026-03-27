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

        bool ValidateAccount(const std::string& accountId, const std::string& password, std::uint64_t& outAccountDbId);
        bool UpsertLoginToken(std::uint64_t accountDbId, const std::string& token, std::uint32_t ttlSeconds, std::uint64_t& outExpiresAtEpoch);

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
