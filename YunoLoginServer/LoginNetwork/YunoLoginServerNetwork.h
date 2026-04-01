#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "TcpServer.h"
#include "MySqlAuthRepository.h"
#include "S2C_AuthResult.h"

namespace yuno::net
{
    class TcpSession;
}

namespace yuno::login
{
    class YunoLoginServerNetwork final
    {
    public:
        YunoLoginServerNetwork();
        ~YunoLoginServerNetwork();

        YunoLoginServerNetwork(const YunoLoginServerNetwork&) = delete;
        YunoLoginServerNetwork& operator=(const YunoLoginServerNetwork&) = delete;

        bool Start(std::uint16_t port);
        void Tick();
        void Stop();

    private:
        struct LoginSessionState
        {
            std::uint64_t sessionId = 0;
            std::uint64_t userId = 0;
            bool authenticated = false;
            std::string username;
        };

        void OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes);
        void OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec);
        void Update(float deltaSeconds);
        void SendLoginAccepted(std::shared_ptr<yuno::net::TcpSession> session, const std::string& token);
        void SendLoginRejected(std::shared_ptr<yuno::net::TcpSession> session, yuno::net::packets::AuthResultCode code, const char* reason);

    private:
        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;
        yuno::login::MySqlAuthRepository m_authRepo;
        std::unordered_map<std::uint64_t, LoginSessionState> m_sessions;
        std::chrono::steady_clock::time_point m_prevTickTime{};
        std::string m_gameHost = "127.0.0.1";
        std::uint16_t m_gamePort = 9000;
        std::uint32_t m_tokenTtlSeconds = 30 * 60;
    };
}
