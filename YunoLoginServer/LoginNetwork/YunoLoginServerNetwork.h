#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "TcpServer.h"

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
            bool authenticated = false;
            std::string accountId;
        };

        void OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes);
        void OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec);
        void Update(float deltaSeconds);
        void SendLoginAccepted(std::shared_ptr<yuno::net::TcpSession> session, std::uint64_t sid);

    private:
        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;
        std::unordered_map<std::uint64_t, LoginSessionState> m_sessions;
        std::chrono::steady_clock::time_point m_prevTickTime{};
    };
}

