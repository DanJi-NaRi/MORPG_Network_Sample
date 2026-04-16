#include "YunoServerNetwork.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <mysql.h>

#include "ByteIO.h"
#include "C2S_AckSnapshot.h"
#include "C2S_EnterWorld.h"
#include "C2S_MoveInput.h"
#include "Net/C2SPackets/C2S_Ping.h"
#include "PacketBuilder.h"
#include "PacketHeader.h"
#include "PacketType.h"
#include "Net/S2CPackets/S2C_Pong.h"
#include "S2C_WorldSnapshot.h"

namespace yuno::server
{
    namespace
    {
        std::string FallbackDisplayName(std::uint64_t userId)
        {
            std::ostringstream oss;
            oss << "Demo" << userId;
            return oss.str();
        }

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

        void DrainMySqlResults(MYSQL* conn)
        {
            if (!conn)
                return;

            while (true)
            {
                MYSQL_RES* result = mysql_store_result(conn);
                if (result)
                    mysql_free_result(result);

                const int next = mysql_next_result(conn);
                if (next != 0)
                    break;
            }
        }
    }

    YunoServerNetwork::YunoServerNetwork()
        : m_io()
        , m_server(m_io)
        , m_prevTickTime(std::chrono::steady_clock::now())
    {
        m_server.SetOnPacket(
            [this](std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes)
            {
                OnPacket(std::move(session), std::move(packetBytes));
            });

        m_server.SetOnDisconnected(
            [this](std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec)
            {
                OnDisconnected(std::move(session), ec);
            });

        RegisterPacketHandlers();
    }

    YunoServerNetwork::~YunoServerNetwork()
    {
        Stop();
    }

    bool YunoServerNetwork::Start(std::uint16_t port)
    {
        if (!ConnectAuthDbFromEnv())
        {
            std::cerr << "[Server] auth DB connect failed.\n";
            return false;
        }

        if (!m_gameplayRepository.ConnectFromEnv())
        {
            std::cerr << "[Server] gameplay DB connect failed: " << m_gameplayRepository.LastError() << "\n";
            DisconnectAuthDb();
            return false;
        }

        yuno::net::TcpServer::ServerOptions options{};
        options.maxSessions = 2048;
        options.enableKeepAlive = true;
        options.enableNoDelay = true;

        const bool ok = m_server.Start(port, options);
        if (!ok)
        {
            std::cerr << "[Server] failed to start. port=" << port << "\n";
            m_gameplayRepository.Disconnect();
            DisconnectAuthDb();
            return false;
        }

        m_prevTickTime = std::chrono::steady_clock::now();
        std::cout << "[Server] started. port=" << port << "\n";
        return true;
    }

    void YunoServerNetwork::Tick()
    {
        while (m_io.poll_one() > 0)
        {
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - m_prevTickTime).count();
        m_prevTickTime = now;

        Update(deltaSeconds);
    }

    void YunoServerNetwork::Stop()
    {
        m_server.Stop();
        m_players.clear();
        m_sessionPacketBudgets.clear();
        m_gameplayRepository.Disconnect();
        DisconnectAuthDb();
        m_nextEntityId = 1;
        m_nextSnapshotId = 1;
        m_serverTick = 0;
        m_snapshotAccumulatorSec = 0.0f;
    }

    std::shared_ptr<yuno::net::TcpSession> YunoServerNetwork::FindSession(std::uint64_t sessionId) const
    {
        return m_server.FindSession(sessionId);
    }

    std::size_t YunoServerNetwork::GetSessionCount() const
    {
        return m_server.GetSessionCount();
    }

    void YunoServerNetwork::OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes)
    {
        if (!session || packetBytes.size() < yuno::net::yunoPacketHeaderSize)
            return;

        const yuno::net::PacketHeader header = yuno::net::UnPackHeaderLE(packetBytes.data());
        const std::uint64_t sid = session->GetSessionId();
        if (!ConsumeSessionPacketBudget(sid, header.type))
            return;

        yuno::net::NetPeer peer{};
        peer.sId = sid;
        if (!m_dispatcher.Dispatch(peer, packetBytes))
        {
            std::cout << "[Server] ignore packet type="
                      << static_cast<unsigned>(static_cast<std::uint8_t>(header.type)) << "\n";
        }
    }

    void YunoServerNetwork::HandleEnterWorld(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_EnterWorld enterWorld{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            enterWorld = yuno::net::packets::C2S_EnterWorld::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        if (enterWorld.loginToken.empty())
        {
            std::cout << "[Server] enter-world rejected sid=" << sid << " reason=EMPTY_TOKEN\n";
            return;
        }

        std::uint64_t userId = 0;
        if (!ValidateLoginToken(enterWorld.loginToken, userId))
        {
            std::cout << "[Server] enter-world rejected sid=" << sid << " reason=INVALID_TOKEN\n";
            return;
        }

        for (const auto& entry : m_players)
        {
            const PlayerRuntimeState& existing = entry.second;
            if (existing.inWorld && existing.userId == userId && existing.sessionId != sid)
            {
                std::cout << "[Server] enter-world rejected sid=" << sid
                          << " reason=ALREADY_IN_WORLD userId=" << userId << "\n";
                return;
            }
        }

        std::uint32_t characterId = 0;
        if (!m_gameplayRepository.EnsureCharacterForUser(userId, characterId))
        {
            std::cout << "[Server] enter-world rejected sid=" << sid
                      << " reason=CHARACTER_SETUP_FAILED error=" << m_gameplayRepository.LastError() << "\n";
            return;
        }

        auto [it, inserted] = m_players.emplace(sid, PlayerRuntimeState{});
        PlayerRuntimeState& player = it->second;
        player.sessionId = sid;
        player.userId = userId;
        player.characterId = characterId;
        player.archetypeId = kBlasterArchetypeId;
        player.loginToken = enterWorld.loginToken;
        player.partyId = 0;
        player.instanceId = 0;
        player.scene = SceneKind::Town;
        player.sceneKey = 0;
        player.hp = player.maxHp = 100;
        player.alive = true;

        if (inserted || player.entityId == 0)
            player.entityId = m_nextEntityId++;

        std::string displayName;
        if (!m_gameplayRepository.LoadCharacterName(characterId, displayName))
            displayName = FallbackDisplayName(userId);
        player.displayName = std::move(displayName);

        const SpawnPoint spawnPoint = m_spawnPointResolver.Resolve(enterWorld.spawnRegionId);
        player.x = spawnPoint.x;
        player.y = spawnPoint.y;
        player.z = spawnPoint.z;
        player.inWorld = true;

        SyncInventoryForPlayer(player);
        SendSpawnEntity(session, player);
        SendInventoryState(player);
        BroadcastWorldSnapshot();

        std::cout << "[Server] enter-world accepted sid=" << sid
                  << " userId=" << player.userId
                  << " characterId=" << player.characterId
                  << " entityId=" << player.entityId
                  << " inventoryCount=" << player.inventory.size()
                  << " gold=" << player.gold
                  << "\n";
    }

    void YunoServerNetwork::SendSpawnEntity(
        std::shared_ptr<yuno::net::TcpSession> session,
        const PlayerRuntimeState& player) const
    {
        if (!session)
            return;

        yuno::net::packets::S2C_SpawnEntity spawn{};
        spawn.entityId = player.entityId;
        spawn.archetypeId = player.archetypeId;
        spawn.x = player.x;
        spawn.y = player.y;
        spawn.z = player.z;
        spawn.displayName = player.displayName;

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_SpawnEntity,
            [&spawn](yuno::net::ByteWriter& w)
            {
                spawn.Serialize(w);
            });

        session->Send(std::move(bytes));
    }

    void YunoServerNetwork::HandleMoveInput(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_MoveInput moveInput{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            moveInput = yuno::net::packets::C2S_MoveInput::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto it = m_players.find(sid);
        if (it == m_players.end())
            return;

        PlayerRuntimeState& player = it->second;
        if (!player.inWorld || player.entityId == 0)
            return;

        if (moveInput.entityId != 0 && moveInput.entityId != player.entityId)
            return;

        for (const auto& frame : moveInput.frames)
        {
            if (frame.sequence <= player.lastProcessedInputSequence)
                continue;

            float inputX = std::clamp(frame.moveX, -1.0f, 1.0f);
            float inputZ = std::clamp(frame.moveY, -1.0f, 1.0f);

            const float inputLenSq = inputX * inputX + inputZ * inputZ;
            if (inputLenSq > 0.0f)
            {
                const float invLen = 1.0f / std::sqrt(inputLenSq);
                inputX *= invLen;
                inputZ *= invLen;

                player.x += inputX * kMoveSpeedUnitsPerSec * kInputStepSeconds;
                player.z += inputZ * kMoveSpeedUnitsPerSec * kInputStepSeconds;
            }

            player.lastProcessedInputSequence = frame.sequence;
        }
    }

    void YunoServerNetwork::HandleAckSnapshot(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_AckSnapshot ack{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            ack = yuno::net::packets::C2S_AckSnapshot::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto it = m_players.find(sid);
        if (it == m_players.end())
            return;

        PlayerRuntimeState& player = it->second;
        if (ack.snapshotId > player.lastAckedSnapshotId)
            player.lastAckedSnapshotId = ack.snapshotId;
    }

    void YunoServerNetwork::HandlePing(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_Ping ping{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            ping = yuno::net::packets::C2S_Ping::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        yuno::net::packets::S2C_Pong pong{};
        pong.reqTime = ping.reqTime;

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_Pong,
            [&pong](yuno::net::ByteWriter& w)
            {
                pong.Serialize(w);
            });

        session->Send(std::move(bytes));
    }

    void YunoServerNetwork::HandlePartyCreate(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            (void)yuno::net::packets::C2S_PartyCreate::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto playerIt = m_players.find(sid);
        if (playerIt == m_players.end() || !playerIt->second.inWorld)
        {
            yuno::net::packets::S2C_PartyState state{};
            state.resultCode = yuno::net::packets::PartyResultCode::NotInWorld;
            SendPartyState(session, state);
            return;
        }

        std::uint32_t partyId = 0;
        const auto code = m_partyManager.CreateParty(sid, partyId);
        if (code != yuno::net::packets::PartyResultCode::None)
        {
            yuno::net::packets::S2C_PartyState state{};
            state.resultCode = code;
            SendPartyState(session, state);
            return;
        }

        playerIt->second.partyId = partyId;
        SendPartyStateToParty(partyId, yuno::net::packets::PartyResultCode::None);
    }

    void YunoServerNetwork::HandlePartyList(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            (void)yuno::net::packets::C2S_PartyList::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        SendPartyList(session);
    }

    void YunoServerNetwork::HandlePartyJoin(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_PartyJoin join{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            join = yuno::net::packets::C2S_PartyJoin::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto playerIt = m_players.find(sid);
        if (playerIt == m_players.end() || !playerIt->second.inWorld)
        {
            yuno::net::packets::S2C_PartyState state{};
            state.resultCode = yuno::net::packets::PartyResultCode::NotInWorld;
            SendPartyState(session, state);
            return;
        }

        const auto code = m_partyManager.JoinParty(sid, join.partyId);
        if (code != yuno::net::packets::PartyResultCode::None)
        {
            yuno::net::packets::S2C_PartyState state{};
            state.resultCode = code;
            state.partyId = join.partyId;
            SendPartyState(session, state);
            return;
        }

        playerIt->second.partyId = join.partyId;
        SendPartyStateToParty(join.partyId, yuno::net::packets::PartyResultCode::None);
    }

    void YunoServerNetwork::HandlePartyLeave(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            (void)yuno::net::packets::C2S_PartyLeave::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto playerIt = m_players.find(sid);
        if (playerIt == m_players.end())
            return;

        const std::uint32_t oldPartyId = playerIt->second.partyId;
        const auto code = m_partyManager.LeaveParty(sid, nullptr);
        playerIt->second.partyId = 0;

        yuno::net::packets::S2C_PartyState selfState{};
        selfState.resultCode = code;
        SendPartyState(session, selfState);

        if (oldPartyId != 0)
            SendPartyStateToParty(oldPartyId, yuno::net::packets::PartyResultCode::None);
    }

    std::vector<InstanceManager::ParticipantSeed> YunoServerNetwork::BuildParticipantSeeds(const PartyManager::Party& party) const
    {
        std::vector<InstanceManager::ParticipantSeed> seeds;
        seeds.reserve(party.members.size());
        for (const std::uint64_t memberSid : party.members)
        {
            auto playerIt = m_players.find(memberSid);
            if (playerIt == m_players.end())
                continue;

            const PlayerRuntimeState& player = playerIt->second;
            if (!player.inWorld || player.entityId == 0)
                continue;

            seeds.push_back({ player.sessionId, player.entityId });
        }
        return seeds;
    }

    void YunoServerNetwork::HandleInstanceEnter(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            (void)yuno::net::packets::C2S_InstanceEnter::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const std::uint64_t sid = session->GetSessionId();
        auto playerIt = m_players.find(sid);
        if (playerIt == m_players.end())
            return;

        const PartyManager::Party* party = m_partyManager.FindPartyByMember(sid);
        if (!party)
        {
            yuno::net::packets::S2C_InstanceState state{};
            state.resultCode = yuno::net::packets::InstanceResultCode::NotInParty;
            auto target = FindSession(sid);
            if (target)
            {
                auto bytes = yuno::net::PacketBuilder::Build(
                    yuno::net::PacketType::S2C_InstanceState,
                    [&state](yuno::net::ByteWriter& w)
                    {
                        state.Serialize(w);
                    });
                target->Send(std::move(bytes));
            }
            return;
        }

        if (party->leaderSessionId != sid)
        {
            yuno::net::packets::S2C_InstanceState state{};
            state.resultCode = yuno::net::packets::InstanceResultCode::NotPartyLeader;
            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_InstanceState,
                [&state](yuno::net::ByteWriter& w)
                {
                    state.Serialize(w);
                });
            session->Send(std::move(bytes));
            return;
        }

        const auto participants = BuildParticipantSeeds(*party);
        const auto enterResult = m_instanceManager.EnterPartyInstance(party->partyId, participants, m_nextEntityId++);
        if (enterResult.code != yuno::net::packets::InstanceResultCode::None || !enterResult.instance)
        {
            yuno::net::packets::S2C_InstanceState state{};
            state.resultCode = enterResult.code;
            state.partyId = party->partyId;
            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_InstanceState,
                [&state](yuno::net::ByteWriter& w)
                {
                    state.Serialize(w);
                });
            session->Send(std::move(bytes));
            return;
        }

        const InstanceManager::Instance& instance = *enterResult.instance;
        for (const auto& participant : instance.participants)
        {
            auto memberIt = m_players.find(participant.sessionId);
            if (memberIt == m_players.end())
                continue;

            PlayerRuntimeState& member = memberIt->second;
            member.instanceId = instance.instanceId;
            member.scene = SceneKind::Instance;
            member.sceneKey = instance.instanceId;
            member.hp = participant.hp;
            member.maxHp = participant.maxHp;
            member.alive = participant.alive;
            member.x = 100.0f + static_cast<float>(participant.entityId % 3u) * 2.0f;
            member.z = 100.0f + static_cast<float>(participant.entityId % 3u) * 2.0f;
        }

        SendInstanceStateToParticipants(instance, yuno::net::packets::InstanceResultCode::None);
        BroadcastWorldSnapshot();
    }

    void YunoServerNetwork::HandleSkillCast(
        std::shared_ptr<yuno::net::TcpSession> session,
        const std::uint8_t* body,
        std::uint32_t bodyLen)
    {
        if (!session || !body)
            return;

        yuno::net::packets::C2S_SkillCast cast{};
        try
        {
            yuno::net::ByteReader reader(body, bodyLen);
            cast = yuno::net::packets::C2S_SkillCast::Deserialize(reader);
            if (reader.Remaining() != 0)
                return;
        }
        catch (...)
        {
            return;
        }

        const auto result = m_instanceManager.CastSkill(
            session->GetSessionId(),
            cast.casterEntityId,
            cast.targetEntityId,
            cast.skillId);
        if (!result.instance || result.code != yuno::net::packets::InstanceResultCode::None)
            return;

        const InstanceManager::Instance& instance = *result.instance;
        for (const auto& participant : instance.participants)
        {
            auto playerIt = m_players.find(participant.sessionId);
            if (playerIt == m_players.end())
                continue;
            playerIt->second.hp = participant.hp;
            playerIt->second.maxHp = participant.maxHp;
            playerIt->second.alive = participant.alive;
        }

        SendCombatEvents(instance, result.events);
        SendInstanceStateToParticipants(instance, yuno::net::packets::InstanceResultCode::None);

        if (!result.resolved)
            return;

        RewardGrantResult rewardSummary{};
        bool haveRewardSummary = false;
        if (result.success)
        {
            for (const auto& participant : instance.participants)
            {
                auto playerIt = m_players.find(participant.sessionId);
                if (playerIt == m_players.end())
                    continue;

                RewardGrantResult reward{};
                if (m_gameplayRepository.GrantDemoDungeonReward(playerIt->second.characterId, reward))
                {
                    SyncInventoryForPlayer(playerIt->second);
                    SendInventoryState(playerIt->second);
                    if (!haveRewardSummary)
                    {
                        rewardSummary = reward;
                        haveRewardSummary = true;
                    }
                }
                else
                {
                    std::cout << "[Server] reward grant failed characterId=" << playerIt->second.characterId
                              << " error=" << m_gameplayRepository.LastError() << "\n";
                }
            }
        }

        SendInstanceResultToParticipants(
            instance,
            yuno::net::packets::InstanceResultCode::None,
            result.success,
            haveRewardSummary ? &rewardSummary : nullptr);
        ReturnInstanceParticipantsToTown(instance);
        m_instanceManager.RemoveInstance(instance.instanceId);
        BroadcastWorldSnapshot();
    }

    void YunoServerNetwork::SendPartyStateToParty(
        std::uint32_t partyId,
        yuno::net::packets::PartyResultCode resultCode)
    {
        const PartyManager::Party* party = m_partyManager.FindPartyById(partyId);
        if (!party)
            return;

        yuno::net::packets::S2C_PartyState state{};
        state.resultCode = resultCode;
        state.partyId = party->partyId;

        auto leaderIt = m_players.find(party->leaderSessionId);
        if (leaderIt != m_players.end())
            state.leaderEntityId = leaderIt->second.entityId;

        for (const std::uint64_t memberSid : party->members)
        {
            auto memberIt = m_players.find(memberSid);
            if (memberIt == m_players.end())
                continue;

            const PlayerRuntimeState& member = memberIt->second;
            yuno::net::packets::PartyMemberState memberState{};
            memberState.entityId = member.entityId;
            memberState.online = member.inWorld ? 1 : 0;
            memberState.alive = member.alive ? 1 : 0;
            memberState.displayName = member.displayName;
            state.members.push_back(memberState);
        }

        for (const std::uint64_t memberSid : party->members)
        {
            auto session = FindSession(memberSid);
            if (session)
                SendPartyState(session, state);
        }
    }

    void YunoServerNetwork::SendPartyList(std::shared_ptr<yuno::net::TcpSession> session) const
    {
        if (!session)
            return;

        yuno::net::packets::S2C_PartyList list{};
        const auto parties = m_partyManager.ListParties();
        list.parties.reserve(parties.size());
        for (const auto& party : parties)
        {
            if (party.members.empty())
                continue;

            yuno::net::packets::PartyListEntry entry{};
            entry.partyId = party.partyId;
            entry.memberCount = static_cast<std::uint16_t>(party.members.size());

            auto leaderIt = m_players.find(party.leaderSessionId);
            if (leaderIt != m_players.end())
            {
                entry.leaderEntityId = leaderIt->second.entityId;
                entry.leaderName = leaderIt->second.displayName;
            }

            list.parties.push_back(std::move(entry));
        }

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_PartyList,
            [&list](yuno::net::ByteWriter& w)
            {
                list.Serialize(w);
            });
        session->Send(std::move(bytes));
    }

    void YunoServerNetwork::SendPartyState(
        std::shared_ptr<yuno::net::TcpSession> session,
        const yuno::net::packets::S2C_PartyState& state) const
    {
        if (!session)
            return;

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_PartyState,
            [&state](yuno::net::ByteWriter& w)
            {
                state.Serialize(w);
            });
        session->Send(std::move(bytes));
    }

    void YunoServerNetwork::SendInstanceStateToParticipants(
        const InstanceManager::Instance& instance,
        yuno::net::packets::InstanceResultCode resultCode)
    {
        yuno::net::packets::S2C_InstanceState state{};
        state.resultCode = resultCode;
        state.partyId = instance.partyId;
        state.instanceId = instance.instanceId;
        state.state = instance.state;
        state.enemyEntityId = instance.enemyEntityId;
        state.enemyHp = instance.enemyHp;
        state.enemyMaxHp = instance.enemyMaxHp;
        for (const auto& participant : instance.participants)
        {
            yuno::net::packets::InstanceParticipantState participantState{};
            participantState.entityId = participant.entityId;
            participantState.hp = participant.hp;
            participantState.maxHp = participant.maxHp;
            participantState.alive = participant.alive ? 1 : 0;
            state.participants.push_back(participantState);
        }

        for (const auto& participant : instance.participants)
        {
            auto session = FindSession(participant.sessionId);
            if (!session)
                continue;

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_InstanceState,
                [&state](yuno::net::ByteWriter& w)
                {
                    state.Serialize(w);
                });
            session->Send(std::move(bytes));
        }
    }

    void YunoServerNetwork::SendInstanceResultToParticipants(
        const InstanceManager::Instance& instance,
        yuno::net::packets::InstanceResultCode resultCode,
        bool success,
        const RewardGrantResult* rewardResult)
    {
        yuno::net::packets::S2C_InstanceResult result{};
        result.success = success ? 1 : 0;
        result.resultCode = resultCode;
        result.instanceId = instance.instanceId;
        if (rewardResult)
        {
            result.rewardItemId = rewardResult->rewardItemId;
            result.rewardItemCode = rewardResult->rewardItemCode;
            result.rewardQuantity = rewardResult->rewardQuantity;
            result.goldAward = rewardResult->goldAward;
        }

        for (const auto& participant : instance.participants)
        {
            auto session = FindSession(participant.sessionId);
            if (!session)
                continue;

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_InstanceResult,
                [&result](yuno::net::ByteWriter& w)
                {
                    result.Serialize(w);
                });
            session->Send(std::move(bytes));
        }
    }

    void YunoServerNetwork::SendCombatEvents(
        const InstanceManager::Instance& instance,
        const std::vector<InstanceManager::CombatEventRecord>& events)
    {
        for (const auto& event : events)
        {
            yuno::net::packets::S2C_CombatEvent packet{};
            packet.eventType = event.type;
            packet.eventSequence = event.sequence;
            packet.sourceEntityId = event.sourceEntityId;
            packet.targetEntityId = event.targetEntityId;
            packet.skillId = event.skillId;
            packet.amount = event.amount;
            packet.targetHp = event.targetHp;
            packet.targetMaxHp = event.targetMaxHp;
            packet.stateFlags = event.stateFlags;

            for (const auto& participant : instance.participants)
            {
                auto session = FindSession(participant.sessionId);
                if (!session)
                    continue;

                auto bytes = yuno::net::PacketBuilder::Build(
                    yuno::net::PacketType::S2C_CombatEvent,
                    [&packet](yuno::net::ByteWriter& w)
                    {
                        packet.Serialize(w);
                    });
                session->Send(std::move(bytes));
            }
        }
    }

    void YunoServerNetwork::SendInventoryState(const PlayerRuntimeState& player) const
    {
        auto session = FindSession(player.sessionId);
        if (!session)
            return;

        yuno::net::packets::S2C_InventoryState state{};
        state.characterId = player.characterId;
        state.gold = player.gold;
        for (const PersistedInventoryItem& item : player.inventory)
        {
            yuno::net::packets::InventoryItemState itemState{};
            itemState.slotNo = item.slotNo;
            itemState.itemId = item.itemId;
            itemState.quantity = item.quantity;
            itemState.itemCode = item.itemCode;
            state.items.push_back(itemState);
        }

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_InventoryState,
            [&state](yuno::net::ByteWriter& w)
            {
                state.Serialize(w);
            });
        session->Send(std::move(bytes));
    }

    void YunoServerNetwork::SyncInventoryForPlayer(PlayerRuntimeState& player)
    {
        std::vector<PersistedInventoryItem> items;
        std::uint32_t gold = 0;
        if (!m_gameplayRepository.LoadInventory(player.characterId, items, gold))
        {
            std::cout << "[Server] inventory load failed characterId=" << player.characterId
                      << " error=" << m_gameplayRepository.LastError() << "\n";
            return;
        }
        player.inventory = std::move(items);
        player.gold = gold;
    }

    void YunoServerNetwork::ReturnInstanceParticipantsToTown(const InstanceManager::Instance& instance)
    {
        const SpawnPoint spawnPoint = m_spawnPointResolver.Resolve(0);
        for (const auto& participant : instance.participants)
        {
            auto playerIt = m_players.find(participant.sessionId);
            if (playerIt == m_players.end())
                continue;

            PlayerRuntimeState& player = playerIt->second;
            player.instanceId = 0;
            player.scene = SceneKind::Town;
            player.sceneKey = 0;
            player.alive = true;
            player.hp = player.maxHp;
            player.x = spawnPoint.x;
            player.y = spawnPoint.y;
            player.z = spawnPoint.z;
        }
    }

    void YunoServerNetwork::BroadcastWorldSnapshot()
    {
        const std::uint32_t serverTick = ++m_serverTick;
        const std::uint32_t snapshotId = m_nextSnapshotId++;

        for (const auto& receiverEntry : m_players)
        {
            const PlayerRuntimeState& receiver = receiverEntry.second;
            if (!receiver.inWorld)
                continue;

            std::vector<yuno::net::packets::EntityPoseState> poses;
            poses.reserve(m_players.size());
            for (const auto& candidateEntry : m_players)
            {
                const PlayerRuntimeState& candidate = candidateEntry.second;
                if (!candidate.inWorld)
                    continue;
                if (candidate.scene != receiver.scene)
                    continue;
                if (candidate.scene == SceneKind::Instance && candidate.sceneKey != receiver.sceneKey)
                    continue;

                yuno::net::packets::EntityPoseState pose{};
                pose.entityId = candidate.entityId;
                pose.stateFlags = candidate.alive ? 0u : 1u;
                pose.lastProcessedInputSequence = candidate.lastProcessedInputSequence;
                pose.x = candidate.x;
                pose.y = candidate.y;
                pose.z = candidate.z;
                pose.yaw = 0.0f;
                pose.vx = 0.0f;
                pose.vy = 0.0f;
                pose.vz = 0.0f;
                poses.push_back(pose);
            }

            auto session = m_server.FindSession(receiver.sessionId);
            if (!session)
                continue;

            yuno::net::packets::S2C_WorldSnapshot snapshot{};
            snapshot.serverTick = serverTick;
            snapshot.snapshotId = snapshotId;
            snapshot.baseSnapshotId = receiver.lastAckedSnapshotId;
            snapshot.entities = poses;

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_WorldSnapshot,
                [&snapshot](yuno::net::ByteWriter& w)
                {
                    snapshot.Serialize(w);
                });
            session->Send(std::move(bytes));
        }
    }

    void YunoServerNetwork::OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        auto playerIt = m_players.find(sid);
        std::uint32_t partyId = 0;
        std::string loginToken;
        if (playerIt != m_players.end())
        {
            partyId = playerIt->second.partyId;
            loginToken = playerIt->second.loginToken;
            m_players.erase(playerIt);
        }

        if (!loginToken.empty() && !RevokeLoginTokenByHash(loginToken))
            std::cout << "[Server] token revoke failed sid=" << sid << "\n";

        m_instanceManager.RemoveDisconnected(sid);
        m_partyManager.RemoveDisconnected(sid);
        m_sessionPacketBudgets.erase(sid);

        if (partyId != 0)
            SendPartyStateToParty(partyId, yuno::net::packets::PartyResultCode::None);

        BroadcastWorldSnapshot();
        std::cout << "[Server] disconnected sid=" << sid << " ec=" << ec.message() << "\n";
    }

    void YunoServerNetwork::RegisterPacketHandlers()
    {
        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_EnterWorld,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandleEnterWorld(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_MoveInput,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandleMoveInput(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_AckSnapshot,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandleAckSnapshot(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_Ping,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandlePing(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_PartyCreate,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandlePartyCreate(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_PartyList,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandlePartyList(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_PartyJoin,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandlePartyJoin(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_PartyLeave,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandlePartyLeave(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_InstanceEnter,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandleInstanceEnter(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_SkillCast,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (session)
                    HandleSkillCast(std::move(session), body, bodyLen);
            });
    }

    bool YunoServerNetwork::ConsumeSessionPacketBudget(std::uint64_t sessionId, yuno::net::PacketType type)
    {
        const auto now = std::chrono::steady_clock::now();
        auto [it, inserted] = m_sessionPacketBudgets.emplace(sessionId, SessionPacketBudget{});
        SessionPacketBudget& budget = it->second;
        if (inserted || budget.windowStart.time_since_epoch().count() == 0)
            budget.windowStart = now;

        if (now - budget.windowStart >= kPacketBudgetWindow)
        {
            budget.windowStart = now;
            budget.totalPacketsInWindow = 0;
            budget.moveInputPacketsInWindow = 0;
            budget.droppedPacketsInWindow = 0;
        }

        if ((budget.totalPacketsInWindow + 1) > kMaxPacketsPerWindow)
        {
            ++budget.droppedPacketsInWindow;
            if (budget.droppedPacketsInWindow > kMaxDropsPerWindow)
                m_server.DisconnectSession(sessionId);
            return false;
        }

        if (type == yuno::net::PacketType::C2S_MoveInput
            && (budget.moveInputPacketsInWindow + 1) > kMaxMoveInputPacketsPerWindow)
        {
            ++budget.droppedPacketsInWindow;
            if (budget.droppedPacketsInWindow > kMaxDropsPerWindow)
                m_server.DisconnectSession(sessionId);
            return false;
        }

        ++budget.totalPacketsInWindow;
        if (type == yuno::net::PacketType::C2S_MoveInput)
            ++budget.moveInputPacketsInWindow;
        return true;
    }

    bool YunoServerNetwork::ConnectAuthDbFromEnv()
    {
        DisconnectAuthDb();

        const std::string host = ReadEnvOrDefault("YUNO_DB_HOST", "127.0.0.1");
        const unsigned int port = ReadEnvPortOrDefault("YUNO_DB_PORT", 3306);
        const std::string user = ReadEnvValue("YUNO_DB_USER");
        const std::string password = ReadEnvValue("YUNO_DB_PASS");
        const std::string database = ReadEnvOrDefault("YUNO_DB_NAME", "yuno_auth");

        if (user.empty())
        {
            std::cerr << "[Server] missing required environment variable: YUNO_DB_USER\n";
            return false;
        }

        if (password.empty())
        {
            std::cerr << "[Server] missing required environment variable: YUNO_DB_PASS\n";
            return false;
        }

        MYSQL* mysql = mysql_init(nullptr);
        if (!mysql)
            return false;

        if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0))
        {
            std::cerr << "[Server] mysql_real_connect failed: " << mysql_error(mysql) << "\n";
            mysql_close(mysql);
            return false;
        }

        m_authDb = mysql;
        return true;
    }

    void YunoServerNetwork::DisconnectAuthDb()
    {
        if (!m_authDb)
            return;

        mysql_close(m_authDb);
        m_authDb = nullptr;
    }

    std::string YunoServerNetwork::EscapeSql(const std::string& input)
    {
        if (!m_authDb)
            return std::string();

        std::string escaped;
        escaped.resize(input.size() * 2 + 1);
        const unsigned long written = mysql_real_escape_string(
            m_authDb,
            escaped.data(),
            input.c_str(),
            static_cast<unsigned long>(input.size()));
        escaped.resize(static_cast<std::size_t>(written));
        return escaped;
    }

    bool YunoServerNetwork::ValidateLoginToken(const std::string& token, std::uint64_t& outUserId)
    {
        outUserId = 0;
        if (!m_authDb || token.empty())
            return false;

        const std::string escaped = EscapeSql(token);
        std::ostringstream oss;
        oss << "CALL sp_auth_validate_login_token('" << escaped << "')";

        if (mysql_query(m_authDb, oss.str().c_str()) != 0)
        {
            std::cerr << "[Server] validate token query failed: " << mysql_error(m_authDb) << "\n";
            DrainMySqlResults(m_authDb);
            return false;
        }

        MYSQL_RES* result = mysql_store_result(m_authDb);
        if (!result)
        {
            DrainMySqlResults(m_authDb);
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        bool legacyPlaintextMatch = false;
        if (row && row[0])
        {
            outUserId = static_cast<std::uint64_t>(std::strtoull(row[0], nullptr, 10));
            legacyPlaintextMatch = (row[1] != nullptr && std::strtoul(row[1], nullptr, 10) != 0UL);
        }

        mysql_free_result(result);
        DrainMySqlResults(m_authDb);

        if (outUserId != 0 && legacyPlaintextMatch)
        {
            std::ostringstream migrateOss;
            migrateOss << "CALL sp_auth_migrate_login_token_hash(" << outUserId << ", '" << escaped << "')";
            if (mysql_query(m_authDb, migrateOss.str().c_str()) != 0)
                std::cerr << "[Server] token hash migrate failed: " << mysql_error(m_authDb) << "\n";
            DrainMySqlResults(m_authDb);
        }

        return outUserId != 0;
    }

    bool YunoServerNetwork::RevokeLoginTokenByHash(const std::string& token)
    {
        if (!m_authDb || token.empty())
            return false;

        const std::string escaped = EscapeSql(token);
        std::ostringstream oss;
        oss << "CALL sp_auth_revoke_login_token_by_hash('" << escaped << "')";

        if (mysql_query(m_authDb, oss.str().c_str()) != 0)
        {
            std::cerr << "[Server] revoke token query failed: " << mysql_error(m_authDb) << "\n";
            DrainMySqlResults(m_authDb);
            return false;
        }

        DrainMySqlResults(m_authDb);
        return true;
    }

    void YunoServerNetwork::Update(float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
            return;

        m_snapshotAccumulatorSec += deltaSeconds;
        if (m_snapshotAccumulatorSec >= kSnapshotIntervalSec)
        {
            m_snapshotAccumulatorSec = 0.0f;
            BroadcastWorldSnapshot();
        }
    }
}
