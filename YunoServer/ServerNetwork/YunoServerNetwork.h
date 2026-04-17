#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CombatPackets.h"
#include "InstanceManager.h"
#include "InstancePackets.h"
#include "InventoryPackets.h"
#include "MySqlGameplayRepository.h"
#include "PacketDispatcher.h"
#include "PacketType.h"
#include "PartyManager.h"
#include "PartyPackets.h"
#include "PlayerSpawnPointResolver.h"
#include "TcpServer.h"

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

        std::shared_ptr<yuno::net::TcpSession> FindSession(std::uint64_t sessionId) const;
        std::size_t GetSessionCount() const;

    private:
        enum class SceneKind : std::uint8_t
        {
            Town = 0,
            Instance = 1,
        };

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
            std::uint32_t characterId = 0;
            std::uint32_t entityId = 0;
            std::uint32_t archetypeId = kBlasterArchetypeId;
            bool inWorld = false;
            bool alive = true;
            std::uint32_t hp = 100;
            std::uint32_t maxHp = 100;
            std::uint32_t gold = 0;
            std::uint32_t partyId = 0;
            std::uint32_t instanceId = 0;
            std::string displayName;
            SceneKind scene = SceneKind::Town;
            std::uint32_t sceneKey = 0;
            std::uint32_t lastAckedSnapshotId = 0;
            std::uint32_t lastProcessedInputSequence = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            std::string loginToken;
            std::vector<PersistedInventoryItem> inventory;
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
        void HandlePartyCreate(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyList(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyJoin(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyInvitePlayer(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyRespondJoinRequest(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyRespondInvite(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyBrowsePlayers(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandlePartyLeave(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandleInstanceEnter(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void HandleSkillCast(std::shared_ptr<yuno::net::TcpSession> session, const std::uint8_t* body, std::uint32_t bodyLen);
        void SendSpawnEntity(std::shared_ptr<yuno::net::TcpSession> session, const PlayerRuntimeState& player) const;
        void BroadcastWorldSnapshot();
        void SendPartyList(std::shared_ptr<yuno::net::TcpSession> session) const;
        void SendPartyStateToParty(std::uint32_t partyId, yuno::net::packets::PartyResultCode resultCode);
        void SendPartyState(std::shared_ptr<yuno::net::TcpSession> session, const yuno::net::packets::S2C_PartyState& state) const;
        void SendPartySocialState(
            std::shared_ptr<yuno::net::TcpSession> session,
            yuno::net::packets::PartyResultCode resultCode,
            const std::string& statusText = std::string()) const;
        void SendPartySocialStateToSession(
            std::uint64_t sessionId,
            yuno::net::packets::PartyResultCode resultCode,
            const std::string& statusText = std::string()) const;
        void RefreshPartyUiStateForAll() const;
        std::uint64_t FindSessionIdByEntityId(std::uint32_t entityId) const;
        void SendInstanceStateToParticipants(const InstanceManager::Instance& instance, yuno::net::packets::InstanceResultCode resultCode);
        void SendInstanceResultToParticipants(
            const InstanceManager::Instance& instance,
            yuno::net::packets::InstanceResultCode resultCode,
            bool success,
            const RewardGrantResult* rewardResult);
        void SendCombatEvents(const InstanceManager::Instance& instance, const std::vector<InstanceManager::CombatEventRecord>& events);
        void SendInventoryState(const PlayerRuntimeState& player) const;
        void SyncInventoryForPlayer(PlayerRuntimeState& player);
        void ReturnInstanceParticipantsToTown(const InstanceManager::Instance& instance);
        std::vector<InstanceManager::ParticipantSeed> BuildParticipantSeeds(const PartyManager::Party& party) const;
        bool ConnectAuthDbFromEnv();
        void DisconnectAuthDb();
        bool ValidateLoginToken(const std::string& token, std::uint64_t& outUserId);
        bool RevokeLoginTokenByHash(const std::string& token);
        std::string EscapeSql(const std::string& input);

        boost::asio::io_context m_io;
        yuno::net::TcpServer m_server;
        yuno::net::PacketDispatcher m_dispatcher{ yuno::net::PacketDispatcher::EndpointRole::Server };
        PlayerSpawnPointResolver m_spawnPointResolver;
        PartyManager m_partyManager;
        InstanceManager m_instanceManager;
        MySqlGameplayRepository m_gameplayRepository;
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
