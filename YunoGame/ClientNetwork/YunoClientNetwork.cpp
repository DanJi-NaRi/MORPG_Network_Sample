#include "pch.h"

#include "YunoClientNetwork.h"

// Client network bridge (transport <-> game/main thread)

#include "PacketBuilder.h"
#include "ByteIO.h"
#include "C2S_AckSnapshot.h"
#include "PacketType.h"
#include "S2C_Pong.h"
#include "S2C_WorldSnapshot.h"
#include "WorldPlayerState.h"

// Packet utilities
#include <chrono>

namespace yuno::game
{
    namespace
    {
        std::uint32_t NowMs32()
        {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            return static_cast<std::uint32_t>(ms & 0xFFFFFFFFull);
        }
    }

    YunoClientNetwork::YunoClientNetwork()
        : m_workGuard(boost::asio::make_work_guard(m_io))
        , m_client(m_io)
    {
        m_serverPeer.sId = 0;

        m_client.SetOnPacket(
            [this](std::vector<std::uint8_t>&& packet)
            {
                // Receive callback runs on network io_context thread.
                PushIncoming(std::move(packet));
            });

        m_client.SetOnDisconnected(
            [this](const boost::system::error_code& /*ec*/)
            {
                // Keep default behavior for disconnect for now.
            });
    }

    YunoClientNetwork::~YunoClientNetwork()
    {
        Stop();
    }

    void YunoClientNetwork::Start(const std::string& host, std::uint16_t port)
    {
        if (m_running.exchange(true))
            return;

        m_io.restart();
        m_workGuard.emplace(boost::asio::make_work_guard(m_io));

        // Run network io_context on dedicated thread.
        m_netThread = std::thread(
            [this]()
            {
                m_io.run();
            });

        // Post connect operation to network thread.
        boost::asio::post(
            m_io,
            [this, host, port]()
            {
                m_client.Connect(host, port);
            });
    }

    void YunoClientNetwork::Stop()
    {
        if (!m_running.exchange(false))
            return;

        // Post disconnect operation to network thread.
        boost::asio::post(
            m_io,
            [this]()
            {
                m_client.Disconnect();
            });

        m_workGuard.reset();
        m_io.stop();

        if (m_netThread.joinable())
            m_netThread.join();

        // Clear pending incoming queue.
        {
            std::lock_guard<std::mutex> lock(m_inMtx);
            m_inQ.clear();
        }
    }

    bool YunoClientNetwork::IsConnected() const
    {
        return m_client.IsConnected();
    }

    void YunoClientNetwork::SendPacket(std::vector<std::uint8_t> packetBytes)
    {
        boost::asio::post(
            m_io,
            [this, bytes = std::move(packetBytes)]() mutable
            {
                m_client.Send(std::move(bytes));
            });
    }

    YunoClientNetwork::SnapshotAckDebugInfo YunoClientNetwork::GetSnapshotAckDebugInfo() const
    {
        SnapshotAckDebugInfo info{};
        info.lastReceivedSnapshotId = m_lastReceivedSnapshotId.load(std::memory_order_relaxed);
        info.lastSentAckSnapshotId = m_lastSentAckSnapshotId.load(std::memory_order_relaxed);
        info.lastServerSeenAckSnapshotId = m_lastServerSeenAckSnapshotId.load(std::memory_order_relaxed);
        return info;
    }

    void YunoClientNetwork::PumpIncoming(float dt)
    {
        // Consume queued packets on main thread.
        std::vector<std::uint8_t> pkt;
        while (PopIncoming(pkt))
        {
            if (m_rawPacketTap)
            {
                m_rawPacketTap(pkt);
            }

            m_dispatcher.Dispatch(m_serverPeer, pkt);
        }
    }

    void YunoClientNetwork::PushIncoming(std::vector<std::uint8_t>&& packetBytes)
    {
        std::lock_guard<std::mutex> lock(m_inMtx);
        m_inQ.emplace_back(std::move(packetBytes));
    }

    bool YunoClientNetwork::PopIncoming(std::vector<std::uint8_t>& out)
    {
        std::lock_guard<std::mutex> lock(m_inMtx);
        if (m_inQ.empty())
            return false;

        out = std::move(m_inQ.front());
        m_inQ.pop_front();
        return true;
    }

    // ------------------------------- MORPG packet handlers -------------------------------
    void YunoClientNetwork::RegisterMatchPacketHandler()
    {
        using namespace yuno::net;

        Dispatcher().RegisterRaw(
            PacketType::S2C_SpawnEntity,
            [](const NetPeer&, const PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                if (!body)
                    return;

                try
                {
                    ByteReader reader(body, bodyLen);
                    const auto spawn = yuno::net::packets::S2C_SpawnEntity::Deserialize(reader);
                    if (reader.Remaining() != 0)
                        return;

                    yuno::game::PublishLocalPlayerSpawn(
                        spawn.entityId,
                        spawn.archetypeId,
                        spawn.x,
                        spawn.y,
                        spawn.z);

                    std::cout << "[Client] spawn entityId=" << spawn.entityId
                        << " archetypeId=" << spawn.archetypeId
                        << " pos=(" << spawn.x << ", " << spawn.y << ", " << spawn.z << ")\n";
                }
                catch (...)
                {
                }
            });

        Dispatcher().RegisterRaw(
            PacketType::S2C_WorldSnapshot,
            [this](const NetPeer&, const PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                if (!body)
                    return;

                try
                {
                    ByteReader reader(body, bodyLen);
                    const auto snapshot = yuno::net::packets::S2C_WorldSnapshot::Deserialize(reader);
                    if (reader.Remaining() != 0)
                        return;
                    m_lastReceivedSnapshotId.store(snapshot.snapshotId, std::memory_order_relaxed);
                    m_lastServerSeenAckSnapshotId.store(snapshot.baseSnapshotId, std::memory_order_relaxed);

                    std::vector<yuno::game::WorldEntityState> worldEntities;
                    worldEntities.reserve(snapshot.entities.size());
                    for (const auto& pose : snapshot.entities)
                    {
                        yuno::game::WorldEntityState entity{};
                        entity.entityId = pose.entityId;
                        entity.lastProcessedInputSequence = pose.lastProcessedInputSequence;
                        entity.x = pose.x;
                        entity.y = pose.y;
                        entity.z = pose.z;
                        worldEntities.push_back(entity);
                    }
                    yuno::game::PublishWorldSnapshot(worldEntities);

                    yuno::net::packets::C2S_AckSnapshot ack{};
                    ack.snapshotId = snapshot.snapshotId;
                    auto ackBytes = yuno::net::PacketBuilder::Build(
                        yuno::net::PacketType::C2S_AckSnapshot,
                        [&ack](yuno::net::ByteWriter& w)
                        {
                            ack.Serialize(w);
                        });
                    this->SendPacket(std::move(ackBytes));
                    m_lastSentAckSnapshotId.store(ack.snapshotId, std::memory_order_relaxed);
                }
                catch (...)
                {
                }
            });

        Dispatcher().RegisterRaw(
            PacketType::S2C_Pong,
            [](const NetPeer&, const PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                if (!body)
                    return;

                try
                {
                    ByteReader reader(body, bodyLen);
                    const auto pong = yuno::net::packets::S2C_Pong::Deserialize(reader);
                    if (reader.Remaining() != 0)
                        return;

                    const std::uint32_t nowMs = NowMs32();
                    const std::uint32_t rttMs = nowMs - pong.reqTime;

                    std::cout << "[Client] pong rtt=" << rttMs << "ms"
                              << " reqTime=" << pong.reqTime << "\n";
                }
                catch (...)
                {
                }
            });
    }
}
