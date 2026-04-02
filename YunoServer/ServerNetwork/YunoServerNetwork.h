#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "TcpServer.h"
#include "PlayerSpawnPointResolver.h"

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
        static constexpr std::uint32_t kBlasterArchetypeId = 1001;
        static constexpr float kMoveSpeedUnitsPerSec = 2.25f;
        static constexpr float kSnapshotIntervalSec = 0.05f;

        struct PlayerRuntimeState
        {
            std::uint64_t sessionId = 0;
            std::uint32_t entityId = 0;
            std::uint32_t archetypeId = kBlasterArchetypeId;
            bool inWorld = false;
            std::int16_t inputMoveX = 0;
            std::int16_t inputMoveY = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        void HandleEnterWorld(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandleMoveInput(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void SendSpawnEntity(std::shared_ptr<yuno::net::TcpSession> session, const PlayerRuntimeState& player) const;
        void BroadcastWorldSnapshot();

        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;
        PlayerSpawnPointResolver m_spawnPointResolver;

        std::unordered_map<std::uint64_t, PlayerRuntimeState> m_players;
        std::uint32_t m_nextEntityId = 1;
        std::uint32_t m_nextSnapshotId = 1;
        std::uint32_t m_serverTick = 0;
        float m_snapshotAccumulatorSec = 0.0f;
        std::chrono::steady_clock::time_point m_prevTickTime{};
    };
}
