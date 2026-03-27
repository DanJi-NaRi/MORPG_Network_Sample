#include "YunoServerNetwork.h"

#include <iostream>

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
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();

        auto it = m_players.find(sid);
        if (it == m_players.end())
        {
            PlayerRuntimeState player{};
            player.sessionId = sid;
            m_players.emplace(sid, player);
        }

        if (!packetBytes.empty())
        {
            // Keep this as the base realtime-server behavior for now: raw packet passthrough.
            session->Send(std::move(packetBytes));
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

        // Placeholder realtime update loop.
        // Keep this minimal so MORPG systems (zone, entity, movement, combat) can be layered here.
        (void)deltaSeconds;
    }
}
