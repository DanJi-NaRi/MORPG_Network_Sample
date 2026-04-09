#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "PacketDispatcher.h"
#include "PacketType.h"
#include "TcpServer.h"
#include "PlayerSpawnPointResolver.h"

namespace yuno::net
{
    class TcpSession;
}

struct MYSQL;

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
        void RegisterPacketHandlers();
        bool ConsumeSessionPacketBudget(std::uint64_t sessionId, yuno::net::PacketType type);

    private:
        static constexpr std::uint32_t kBlasterArchetypeId = 1001;
        static constexpr float kMoveSpeedUnitsPerSec = 10.0f;
        static constexpr float kSnapshotIntervalSec = 0.05f;
        static constexpr float kInputStepSeconds = 1.0f / 30.0f;

        struct PlayerRuntimeState
        {
            std::uint64_t sessionId = 0;
            std::uint64_t userId = 0;
            std::uint32_t entityId = 0;
            std::uint32_t archetypeId = kBlasterArchetypeId;
            bool inWorld = false;
            std::uint32_t lastAckedSnapshotId = 0;
            std::uint32_t lastProcessedInputSequence = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            std::string loginToken;
        };

        struct SessionPacketBudget
        {
            std::chrono::steady_clock::time_point windowStart{};
            std::uint32_t totalPacketsInWindow = 0;
            std::uint32_t moveInputPacketsInWindow = 0;
            std::uint32_t droppedPacketsInWindow = 0;
        };

        void HandleEnterWorld(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandleMoveInput(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandleAckSnapshot(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePing(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void SendSpawnEntity(std::shared_ptr<yuno::net::TcpSession> session, const PlayerRuntimeState& player) const;
        void BroadcastWorldSnapshot();
        bool ConnectAuthDbFromEnv();
        void DisconnectAuthDb();
        bool ValidateLoginToken(const std::string& token, std::uint64_t& outUserId);
        bool RevokeLoginTokenByHash(const std::string& token);
        std::string EscapeSql(const std::string& input);

        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;
        yuno::net::PacketDispatcher m_dispatcher{ yuno::net::PacketDispatcher::EndpointRole::Server };
        PlayerSpawnPointResolver m_spawnPointResolver;
        MYSQL* m_authDb = nullptr;

        std::unordered_map<std::uint64_t, PlayerRuntimeState> m_players;
        std::unordered_map<std::uint64_t, SessionPacketBudget> m_sessionPacketBudgets;
        std::uint32_t m_nextEntityId = 1;
        std::uint32_t m_nextSnapshotId = 1;
        std::uint32_t m_serverTick = 0;
        float m_snapshotAccumulatorSec = 0.0f;
        std::chrono::steady_clock::time_point m_prevTickTime{};

        static constexpr std::chrono::seconds kPacketBudgetWindow{ 1 };
        static constexpr std::uint32_t kMaxPacketsPerWindow = 180;
        static constexpr std::uint32_t kMaxMoveInputPacketsPerWindow = 90;
        static constexpr std::uint32_t kMaxDropsPerWindow = 24;
    };
}
