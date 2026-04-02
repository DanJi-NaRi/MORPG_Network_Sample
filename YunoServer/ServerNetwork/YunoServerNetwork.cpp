#include "YunoServerNetwork.h"

#include <cmath>
#include <iostream>

#include "ByteIO.h"
#include "C2S_EnterWorld.h"
#include "C2S_MoveInput.h"
#include "PacketBuilder.h"
#include "PacketHeader.h"
#include "PacketType.h"
#include "S2C_WorldSnapshot.h"

namespace yuno::server
{
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
    }

    YunoServerNetwork::~YunoServerNetwork()
    {
        Stop();
    }

    bool YunoServerNetwork::Start(std::uint16_t port)
    {
        const bool ok = m_server.Start(port);
        if (!ok)
        {
            std::cerr << "[Server] failed to start. port=" << port << "\n";
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
        m_nextEntityId = 1;
        m_nextSnapshotId = 1;
        m_serverTick = 0;
        m_snapshotAccumulatorSec = 0.0f;
    }

    std::shared_ptr<yuno::net::TcpSession> YunoServerNetwork::FindSession(std::uint64_t sessionId)
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
        const std::size_t expectedSize =
            yuno::net::yunoPacketHeaderSize + static_cast<std::size_t>(header.bodyLength);
        if (packetBytes.size() != expectedSize)
            return;

        const std::uint8_t* body = packetBytes.data() + yuno::net::yunoPacketHeaderSize;
        const std::uint32_t bodyLen = header.bodyLength;

        if (header.type == yuno::net::PacketType::C2S_EnterWorld)
        {
            HandleEnterWorld(std::move(session), body, bodyLen);
        }
        else if (header.type == yuno::net::PacketType::C2S_MoveInput)
        {
            HandleMoveInput(std::move(session), body, bodyLen);
        }
        else
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

        auto [it, inserted] = m_players.emplace(sid, PlayerRuntimeState{});
        PlayerRuntimeState& player = it->second;
        player.sessionId = sid;
        player.archetypeId = kBlasterArchetypeId;

        if (inserted || player.entityId == 0)
        {
            player.entityId = m_nextEntityId++;
        }

        const SpawnPoint spawnPoint = m_spawnPointResolver.Resolve(enterWorld.spawnRegionId);
        player.x = spawnPoint.x;
        player.y = spawnPoint.y;
        player.z = spawnPoint.z;
        player.inWorld = true;

        SendSpawnEntity(session, player);
        BroadcastWorldSnapshot();

        std::cout << "[Server] enter-world accepted sid=" << sid
            << " entityId=" << player.entityId
            << " archetype=Blaster(" << player.archetypeId << ")"
            << " region=" << enterWorld.spawnRegionId
            << " pos=(" << player.x << ", " << player.y << ", " << player.z << ")\n";
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
            if (frame.moveX > 0)
                player.inputMoveX = 1;
            else if (frame.moveX < 0)
                player.inputMoveX = -1;
            else
                player.inputMoveX = 0;

            if (frame.moveY > 0)
                player.inputMoveY = 1;
            else if (frame.moveY < 0)
                player.inputMoveY = -1;
            else
                player.inputMoveY = 0;
        }
    }

    void YunoServerNetwork::BroadcastWorldSnapshot()
    {
        yuno::net::packets::S2C_WorldSnapshot snapshot{};
        snapshot.serverTick = ++m_serverTick;
        snapshot.snapshotId = m_nextSnapshotId++;
        snapshot.baseSnapshotId = 0;

        for (const auto& entry : m_players)
        {
            const PlayerRuntimeState& player = entry.second;
            if (!player.inWorld)
                continue;

            yuno::net::packets::EntityPoseState pose{};
            pose.entityId = player.entityId;
            pose.stateFlags = 0;
            pose.x = player.x;
            pose.y = player.y;
            pose.z = player.z;
            pose.yaw = 0.0f;
            pose.vx = 0.0f;
            pose.vy = 0.0f;
            pose.vz = 0.0f;
            snapshot.entities.push_back(pose);
        }

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_WorldSnapshot,
            [&snapshot](yuno::net::ByteWriter& w)
            {
                snapshot.Serialize(w);
            });

        for (const auto& entry : m_players)
        {
            const PlayerRuntimeState& player = entry.second;
            if (!player.inWorld)
                continue;

            auto session = m_server.FindSession(player.sessionId);
            if (!session)
                continue;

            session->Send(bytes);
        }
    }

    void YunoServerNetwork::OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        m_players.erase(sid);

        std::cout << "[Server] disconnected sid=" << sid << " ec=" << ec.message() << "\n";
    }

    void YunoServerNetwork::Update(float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
            return;

        m_snapshotAccumulatorSec += deltaSeconds;
        const bool shouldSendSnapshot = (m_snapshotAccumulatorSec >= kSnapshotIntervalSec);
        if (shouldSendSnapshot)
        {
            m_snapshotAccumulatorSec = 0.0f;
        }

        for (auto& entry : m_players)
        {
            PlayerRuntimeState& player = entry.second;
            if (!player.inWorld)
                continue;

            const float inputX = static_cast<float>(player.inputMoveX);
            const float inputZ = static_cast<float>(player.inputMoveY);
            const float inputLenSq = inputX * inputX + inputZ * inputZ;

            if (inputLenSq > 0.0f)
            {
                const float invLen = 1.0f / std::sqrt(inputLenSq);
                const float dirX = inputX * invLen;
                const float dirZ = inputZ * invLen;

                player.x += dirX * kMoveSpeedUnitsPerSec * deltaSeconds;
                player.z += dirZ * kMoveSpeedUnitsPerSec * deltaSeconds;
            }
        }

        if (shouldSendSnapshot)
        {
            BroadcastWorldSnapshot();
        }
    }
}
