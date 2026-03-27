#include "YunoLoginServerNetwork.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <utility>

namespace yuno::login
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

        std::uint16_t ReadEnvPortOrDefault(const char* name, std::uint16_t fallback)
        {
            const std::string v = ReadEnvValue(name);
            if (v.empty())
                return fallback;

            const unsigned long parsed = std::strtoul(v.c_str(), nullptr, 10);
            if (parsed == 0 || parsed > 65535UL)
                return fallback;

            return static_cast<std::uint16_t>(parsed);
        }

        std::uint32_t ReadEnvU32OrDefault(const char* name, std::uint32_t fallback)
        {
            const std::string v = ReadEnvValue(name);
            if (v.empty())
                return fallback;

            const unsigned long parsed = std::strtoul(v.c_str(), nullptr, 10);
            if (parsed == 0 || parsed > 0xFFFFFFFFUL)
                return fallback;

            return static_cast<std::uint32_t>(parsed);
        }

        std::vector<std::string> SplitByPipe(const std::string& text)
        {
            std::vector<std::string> out;
            std::string current;

            for (char ch : text)
            {
                if (ch == '|')
                {
                    out.push_back(current);
                    current.clear();
                    continue;
                }

                current.push_back(ch);
            }

            out.push_back(current);
            return out;
        }

        std::string MakeLoginToken(std::uint64_t sid)
        {
            static std::random_device rd;
            static std::mt19937_64 rng(rd());

            std::ostringstream oss;
            oss << std::hex << std::setfill('0');

            for (int i = 0; i < 3; ++i)
            {
                const std::uint64_t value = rng();
                oss << std::setw(16) << value;
            }

            oss << std::setw(16) << sid;
            return oss.str();
        }

        void SendAsciiPacket(std::shared_ptr<yuno::net::TcpSession> session, std::uint8_t packetType, const std::string& payload)
        {
            if (!session)
                return;

            std::vector<std::uint8_t> packet;
            packet.resize(8 + payload.size());

            const std::uint32_t bodyLen = static_cast<std::uint32_t>(payload.size());
            packet[0] = static_cast<std::uint8_t>(bodyLen & 0xFF);
            packet[1] = static_cast<std::uint8_t>((bodyLen >> 8) & 0xFF);
            packet[2] = static_cast<std::uint8_t>((bodyLen >> 16) & 0xFF);
            packet[3] = static_cast<std::uint8_t>((bodyLen >> 24) & 0xFF);
            packet[4] = packetType;
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

    YunoLoginServerNetwork::YunoLoginServerNetwork()
        : m_io()
        , m_server(m_io)
        , m_prevTickTime(std::chrono::steady_clock::now())
    {
        m_gameHost = ReadEnvOrDefault("YUNO_GAME_HOST", "127.0.0.1");
        m_gamePort = ReadEnvPortOrDefault("YUNO_GAME_PORT", 9000);
        m_tokenTtlSeconds = ReadEnvU32OrDefault("YUNO_LOGIN_TOKEN_TTL", 30 * 60);

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
        if (!m_authRepo.ConnectFromEnv())
        {
            std::cerr << "[LoginServer] DB connect failed: " << m_authRepo.LastError() << "\n";
            return false;
        }

        const bool ok = m_server.Start(port);
        if (!ok)
        {
            std::cerr << "[LoginServer] failed to start. port=" << port << "\n";
            return false;
        }

        m_prevTickTime = std::chrono::steady_clock::now();
        std::cout << "[LoginServer] started. port=" << port
                  << " game=" << m_gameHost << ":" << m_gamePort << "\n";
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
        m_authRepo.Disconnect();
    }

    void YunoLoginServerNetwork::OnPacket(std::shared_ptr<yuno::net::TcpSession> session, std::vector<std::uint8_t>&& packetBytes)
    {
        if (!session)
            return;

        const std::uint64_t sid = session->GetSessionId();
        auto& state = m_sessions[sid];
        state.sessionId = sid;

        if (packetBytes.empty())
        {
            SendLoginRejected(session, "EMPTY");
            return;
        }

        if (state.authenticated)
            return;

        const std::string request(packetBytes.begin(), packetBytes.end());
        const std::vector<std::string> parts = SplitByPipe(request);
        if (parts.size() < 3 || parts[0] != "LOGIN")
        {
            SendLoginRejected(session, "FORMAT");
            return;
        }

        const std::string& accountId = parts[1];
        const std::string& password = parts[2];

        if (accountId.empty() || password.empty())
        {
            SendLoginRejected(session, "EMPTY_CREDENTIAL");
            return;
        }

        std::uint64_t accountDbId = 0;
        if (!m_authRepo.ValidateAccount(accountId, password, accountDbId))
        {
            SendLoginRejected(session, "AUTH");
            std::cout << "[LoginServer] auth failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
            return;
        }

        const std::string token = MakeLoginToken(sid);
        std::uint64_t expiresAtEpoch = 0;
        if (!m_authRepo.UpsertLoginToken(accountDbId, token, m_tokenTtlSeconds, expiresAtEpoch))
        {
            SendLoginRejected(session, "DB");
            std::cout << "[LoginServer] token issue failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
            return;
        }

        state.authenticated = true;
        state.accountId = accountId;

        SendLoginAccepted(session, sid, token, accountDbId, expiresAtEpoch);

        std::cout << "[LoginServer] login accepted sid=" << sid
                  << " account=" << accountId
                  << " accountDbId=" << accountDbId << "\n";
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

    void YunoLoginServerNetwork::SendLoginAccepted(
        std::shared_ptr<yuno::net::TcpSession> session,
        std::uint64_t sid,
        const std::string& token,
        std::uint64_t accountDbId,
        std::uint64_t expiresAtEpoch)
    {
        const std::string payload =
            "OK|"
            + m_gameHost + "|"
            + std::to_string(m_gamePort) + "|"
            + token + "|"
            + std::to_string(accountDbId) + "|"
            + std::to_string(expiresAtEpoch) + "|"
            + std::to_string(sid);

        SendAsciiPacket(session, 1, payload);
    }

    void YunoLoginServerNetwork::SendLoginRejected(std::shared_ptr<yuno::net::TcpSession> session, const char* reason)
    {
        const std::string payload = "ERR|" + std::string(reason ? reason : "UNKNOWN");
        SendAsciiPacket(session, 2, payload);
    }
}
