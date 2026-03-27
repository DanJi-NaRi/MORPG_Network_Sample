#include "YunoLoginServerNetwork.h"

#include <iostream>
#include <utility>

namespace yuno::login
{
    YunoLoginServerNetwork::YunoLoginServerNetwork()
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

    YunoLoginServerNetwork::~YunoLoginServerNetwork()
    {
        Stop();
    }

    bool YunoLoginServerNetwork::Start(std::uint16_t port)
    {
        const bool ok = m_server.Start(port);
        if (!ok)
        {
            std::cerr << "[LoginServer] failed to start. port=" << port << "\n";
            return false;
        }

        m_prevTickTime = std::chrono::steady_clock::now();
        std::cout << "[LoginServer] started. port=" << port << "\n";
        return true;
    }

    void YunoLoginServerNetwork::Tick()
    {
        while (m_io.poll_one() > 0)
        {
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - m_prevTickTime).count();
        m_prevTickTime = now;

        Update(deltaSeconds);
    }

    void YunoLoginServerNetwork::Stop()
    {
        m_server.Stop();
        m_sessions.clear();
    }

    void YunoLoginServerNetwork::OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        auto& state = m_sessions[sid];
        state.sessionId = sid;

        // Placeholder flow:
        // Any non-empty login packet is accepted for now and receives a simple response frame.
        if (!packetBytes.empty() && !state.authenticated)
        {
            state.authenticated = true;
            state.accountId = "dev-account";
            SendLoginAccepted(session, sid);

            std::cout << "[LoginServer] login accepted sid=" << sid << "\n";
        }
    }

    void YunoLoginServerNetwork::OnDisconnected(std::shared_ptr<yuno::net::TcpSession> session, const boost::system::error_code& ec)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        m_sessions.erase(sid);

        std::cout << "[LoginServer] disconnected sid=" << sid << " ec=" << ec.message() << "\n";
    }

    void YunoLoginServerNetwork::Update(float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
            return;

        (void)deltaSeconds;
    }

    void YunoLoginServerNetwork::SendLoginAccepted(std::shared_ptr<yuno::net::TcpSession> session, std::uint64_t sid)
    {
        if (!session)
            return;

        // Reuse transport framing:
        // [0..3] bodyLen LE, [4..5] type, [6..7] flags, then ASCII payload body.
        // payload format (temporary): "OK|127.0.0.1|9000|token-dev-<sid>"
        std::string payload = "OK|127.0.0.1|9000|token-dev-" + std::to_string(sid);
        std::vector<std::uint8_t> packet;
        packet.resize(8 + payload.size());

        const std::uint32_t bodyLen = static_cast<std::uint32_t>(payload.size());
        packet[0] = static_cast<std::uint8_t>(bodyLen & 0xFF);
        packet[1] = static_cast<std::uint8_t>((bodyLen >> 8) & 0xFF);
        packet[2] = static_cast<std::uint8_t>((bodyLen >> 16) & 0xFF);
        packet[3] = static_cast<std::uint8_t>((bodyLen >> 24) & 0xFF);
        packet[4] = 1; // login-accepted temp type
        packet[5] = 0;
        packet[6] = 0;
        packet[7] = 0;

        for (std::size_t i = 0; i < payload.size(); ++i)
        {
            packet[8 + i] = static_cast<std::uint8_t>(payload[i]);
        }

        session->Send(std::move(packet));
    }
}

