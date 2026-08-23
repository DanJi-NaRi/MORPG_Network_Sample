#include "YunoServerNetwork.h"

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
                {
                    mysql_free_result(result);
                }

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

        yuno::net::TcpServer::ServerOptions options{};
        options.maxSessions = 2048;
        options.enableKeepAlive = true;
        options.enableNoDelay = true;

        const bool ok = m_server.Start(port, options);
        if (!ok)
        {
            std::cerr << "[Server] failed to start. port=" << port << "\n";
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
        DisconnectAuthDb();
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

        auto [it, inserted] = m_players.emplace(sid, PlayerRuntimeState{});
        PlayerRuntimeState& player = it->second;
        player.sessionId = sid;
        player.userId = userId;
        player.archetypeId = kBlasterArchetypeId;
        player.loginToken = enterWorld.loginToken;

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
            << " userId=" << player.userId
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
        {
            player.lastAckedSnapshotId = ack.snapshotId;
        }
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

    void YunoServerNetwork::BroadcastWorldSnapshot()
    {
        const std::uint32_t serverTick = ++m_serverTick;
        const std::uint32_t snapshotId = m_nextSnapshotId++;
        std::vector<yuno::net::packets::EntityPoseState> poses;
        poses.reserve(m_players.size());

        for (const auto& entry : m_players)
        {
            const PlayerRuntimeState& player = entry.second;
            if (!player.inWorld)
                continue;

            yuno::net::packets::EntityPoseState pose{};
            pose.entityId = player.entityId;
            pose.stateFlags = 0;
            pose.lastProcessedInputSequence = player.lastProcessedInputSequence;
            pose.x = player.x;
            pose.y = player.y;
            pose.z = player.z;
            pose.yaw = 0.0f;
            pose.vx = 0.0f;
            pose.vy = 0.0f;
            pose.vz = 0.0f;
            poses.push_back(pose);
        }

        for (const auto& entry : m_players)
        {
            const PlayerRuntimeState& player = entry.second;
            if (!player.inWorld)
                continue;

            auto session = m_server.FindSession(player.sessionId);
            if (!session)
                continue;

            yuno::net::packets::S2C_WorldSnapshot snapshot{};
            snapshot.serverTick = serverTick;
            snapshot.snapshotId = snapshotId;
            snapshot.baseSnapshotId = player.lastAckedSnapshotId;
            snapshot.entities = poses;

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_WorldSnapshot,
                [&snapshot](yuno::net::ByteWriter& w)
                {
                    snapshot.Serialize(w);
                });
            session->Send(bytes);
        }
    }

    void YunoServerNetwork::OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        auto it = m_players.find(sid);
        if (it != m_players.end())
        {
            if (!it->second.loginToken.empty() && !RevokeLoginTokenByHash(it->second.loginToken))
            {
                std::cout << "[Server] token revoke failed sid=" << sid << "\n";
            }
            m_players.erase(it);
        }
        m_sessionPacketBudgets.erase(sid);

        std::cout << "[Server] disconnected sid=" << sid << " ec=" << ec.message() << "\n";
    }

    void YunoServerNetwork::RegisterPacketHandlers()
    {
        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_EnterWorld,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (!session)
                    return;
                HandleEnterWorld(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_MoveInput,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (!session)
                    return;
                HandleMoveInput(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_AckSnapshot,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (!session)
                    return;
                HandleAckSnapshot(std::move(session), body, bodyLen);
            });

        m_dispatcher.RegisterRaw(
            yuno::net::PacketType::C2S_Ping,
            [this](const yuno::net::NetPeer& peer, const yuno::net::PacketHeader&, const std::uint8_t* body, std::uint32_t bodyLen)
            {
                auto session = FindSession(peer.sId);
                if (!session)
                    return;
                HandlePing(std::move(session), body, bodyLen);
            });
    }

    bool YunoServerNetwork::ConsumeSessionPacketBudget(std::uint64_t sessionId, yuno::net::PacketType type)
    {
        const auto now = std::chrono::steady_clock::now();
        auto [it, inserted] = m_sessionPacketBudgets.emplace(sessionId, SessionPacketBudget{});
        SessionPacketBudget& budget = it->second;
        if (inserted || budget.windowStart.time_since_epoch().count() == 0)
        {
            budget.windowStart = now;
        }

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
            {
                m_server.DisconnectSession(sessionId);
            }
            return false;
        }

        if (type == yuno::net::PacketType::C2S_MoveInput
            && (budget.moveInputPacketsInWindow + 1) > kMaxMoveInputPacketsPerWindow)
        {
            ++budget.droppedPacketsInWindow;
            if (budget.droppedPacketsInWindow > kMaxDropsPerWindow)
            {
                m_server.DisconnectSession(sessionId);
            }
            return false;
        }

        ++budget.totalPacketsInWindow;
        if (type == yuno::net::PacketType::C2S_MoveInput)
        {
            ++budget.moveInputPacketsInWindow;
        }
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

        if (user.empty() || password.empty())
        {
            std::cerr << "[Server] YUNO_DB_USER and YUNO_DB_PASS environment variables are required.\n";
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
            migrateOss << "CALL sp_auth_migrate_login_token_hash("
                       << outUserId
                       << ", '"
                       << escaped
                       << "')";

            if (mysql_query(m_authDb, migrateOss.str().c_str()) != 0)
            {
                std::cerr << "[Server] token hash migrate failed: " << mysql_error(m_authDb) << "\n";
            }
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
        const bool shouldSendSnapshot = (m_snapshotAccumulatorSec >= kSnapshotIntervalSec);
        if (shouldSendSnapshot)
        {
            m_snapshotAccumulatorSec = 0.0f;
        }

        if (shouldSendSnapshot)
        {
            BroadcastWorldSnapshot();
        }
    }
}
