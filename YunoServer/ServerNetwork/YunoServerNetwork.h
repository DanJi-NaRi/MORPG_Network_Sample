#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "TcpServer.h"

namespace yuno::net
{
    class TcpSession;
}

namespace yuno::server
{
    class YunoServerNetwork final
    {
    public:
        YunoServerNetwork();
        ~YunoServerNetwork();

        YunoServerNetwork(const YunoServerNetwork&) = delete;
        YunoServerNetwork& operator=(const YunoServerNetwork&) = delete;

        bool Start(std::uint16_t port);
        void Tick();
        void Stop();

        std::shared_ptr<yuno::net::TcpSession> FindSession(std::uint64_t sessionId);
        std::size_t GetSessionCount() const;

    private:
        void OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes);
        void OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec);
        void Update(float deltaSeconds);

    private:
        struct PlayerRuntimeState
        {
            std::uint64_t sessionId = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;

        std::unordered_map<std::uint64_t, PlayerRuntimeState> m_players;
        std::chrono::steady_clock::time_point m_prevTickTime{};
    };
}
