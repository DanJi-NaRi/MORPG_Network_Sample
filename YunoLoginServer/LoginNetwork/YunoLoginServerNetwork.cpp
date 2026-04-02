#include "YunoLoginServerNetwork.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "ByteIO.h"
#include "C2S_AuthHello.h"
#include "C2S_AuthLogout.h"
#include "C2S_AuthRegister.h"
#include "PacketBuilder.h"
#include "PacketHeader.h"
#include "PacketType.h"
#include "S2C_AuthResult.h"

namespace yuno::login
{
    namespace
    {
        enum class AuthOp : std::uint8_t
        {
            Login = 1,
            Register = 2,
            Logout = 3,
        };

        struct AuthRequest
        {
            AuthOp op = AuthOp::Login;
            std::string loginId;
            std::string password;
            std::string token;
        };

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

        bool TryParseAuthRequest(const std::vector<std::uint8_t>& packetBytes, AuthRequest& outReq)
        {
            if (packetBytes.size() < yuno::net::yunoPacketHeaderSize)
                return false;

            const yuno::net::PacketHeader header = yuno::net::UnPackHeaderLE(packetBytes.data());

            const std::size_t expectedSize =
                yuno::net::yunoPacketHeaderSize + static_cast<std::size_t>(header.bodyLength);
            if (packetBytes.size() != expectedSize)
                return false;

            try
            {
                yuno::net::ByteReader reader(packetBytes.data() + yuno::net::yunoPacketHeaderSize, header.bodyLength);
                if (header.type == yuno::net::PacketType::C2S_AuthHello)
                {
                    const yuno::net::packets::C2S_AuthHello hello = yuno::net::packets::C2S_AuthHello::Deserialize(reader);
                    outReq.op = AuthOp::Login;
                    outReq.loginId = hello.loginId;
                    outReq.password = hello.password;
                    return reader.Remaining() == 0;
                }

                if (header.type == yuno::net::PacketType::C2S_AuthRegister)
                {
                    const yuno::net::packets::C2S_AuthRegister reg = yuno::net::packets::C2S_AuthRegister::Deserialize(reader);
                    outReq.op = AuthOp::Register;
                    outReq.loginId = reg.loginId;
                    outReq.password = reg.password;
                    return reader.Remaining() == 0;
                }

                if (header.type == yuno::net::PacketType::C2S_AuthLogout)
                {
                    const yuno::net::packets::C2S_AuthLogout logoutReq = yuno::net::packets::C2S_AuthLogout::Deserialize(reader);
                    outReq.op = AuthOp::Logout;
                    outReq.token = logoutReq.token;
                    return reader.Remaining() == 0;
                }

                return false;
            }
            catch (const std::exception&)
            {
                return false;
            }
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
        std::cout << "[LoginServer] packet received sid=" << sid << " bytes=" << packetBytes.size() << "\n";
        auto& state = m_sessions[sid];
        state.sessionId = sid;

        if (state.authenticated)
            return;

        AuthRequest request;
        if (!TryParseAuthRequest(packetBytes, request))
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::Format, "FORMAT");
            return;
        }

        const std::string& username = request.loginId;
        const std::string& password = request.password;

        if (request.op == AuthOp::Logout)
        {
            if (request.token.empty())
            {
                SendLoginRejected(session, yuno::net::packets::AuthResultCode::Format, "EMPTY_TOKEN");
                return;
            }

            if (!m_authRepo.RevokeLoginTokenByHash(request.token))
            {
                SendLoginRejected(session, yuno::net::packets::AuthResultCode::DbError, "DB");
                std::cout << "[LoginServer] logout failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
                return;
            }

            yuno::net::packets::S2C_AuthResult result{};
            result.success = 1;
            result.code = yuno::net::packets::AuthResultCode::None;
            result.message = "LOGOUT_OK";

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_AuthResult,
                [&result](yuno::net::ByteWriter& w)
                {
                    result.Serialize(w);
                });

            session->Send(std::move(bytes));
            return;
        }

        if (username.empty() || password.empty())
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::EmptyCredential, "EMPTY_CREDENTIAL");
            return;
        }

        if (request.op == AuthOp::Register)
        {
            bool alreadyExists = false;
            if (!m_authRepo.CreateUser(username, password, alreadyExists))
            {
                if (alreadyExists)
                {
                    SendLoginRejected(session, yuno::net::packets::AuthResultCode::AccountExists, "ACCOUNT_EXISTS");
                    return;
                }

                SendLoginRejected(session, yuno::net::packets::AuthResultCode::RegisterFailed, "REGISTER_FAILED");
                std::cout << "[LoginServer] register failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
                return;
            }

            yuno::net::packets::S2C_AuthResult result{};
            result.success = 1;
            result.code = yuno::net::packets::AuthResultCode::None;
            result.message = "REGISTER_OK";

            auto bytes = yuno::net::PacketBuilder::Build(
                yuno::net::PacketType::S2C_AuthResult,
                [&result](yuno::net::ByteWriter& w)
                {
                    result.Serialize(w);
                });

            session->Send(std::move(bytes));
            std::cout << "[LoginServer] register success sid=" << sid << " username=" << username << "\n";
            return;
        }

        std::uint64_t userId = 0;
        if (!m_authRepo.ValidateUserCredentials(username, password, userId))
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::AuthFailed, "AUTH");
            std::cout << "[LoginServer] auth failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
            return;
        }

        bool hasActiveToken = false;
        if (!m_authRepo.HasActiveLoginToken(userId, hasActiveToken))
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::DbError, "DB");
            std::cout << "[LoginServer] active token check failed sid=" << sid << " userId=" << userId
                      << " reason=" << m_authRepo.LastError() << "\n";
            return;
        }
        if (hasActiveToken)
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::AlreadyLoggedIn, "ALREADY_LOGIN");
            std::cout << "[LoginServer] already login sid=" << sid << " userId=" << userId << "\n";
            return;
        }

        const std::string token = MakeLoginToken(sid);
        std::uint64_t expiresAtEpoch = 0;
        if (!m_authRepo.UpsertLoginToken(userId, token, m_tokenTtlSeconds, expiresAtEpoch))
        {
            SendLoginRejected(session, yuno::net::packets::AuthResultCode::DbError, "DB");
            std::cout << "[LoginServer] token issue failed sid=" << sid << " reason=" << m_authRepo.LastError() << "\n";
            return;
        }

        if (!m_authRepo.TouchLastLogin(userId))
        {
            std::cout << "[LoginServer] last_login_at update failed userId=" << userId
                      << " reason=" << m_authRepo.LastError() << "\n";
        }

        state.authenticated = true;
        state.userId = userId;
        state.username = username;

        (void)expiresAtEpoch;
        SendLoginAccepted(session, token);

        std::cout << "[LoginServer] login accepted sid=" << sid
                  << " username=" << username
                  << " userId=" << userId << "\n";
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
        const std::string& token)
    {
        yuno::net::packets::S2C_AuthResult result{};
        result.success = 1;
        result.code = yuno::net::packets::AuthResultCode::None;
        result.message = "OK";
        result.gameHost = m_gameHost;
        result.gamePort = m_gamePort;
        result.loginToken = token;

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_AuthResult,
            [&result](yuno::net::ByteWriter& w)
            {
                result.Serialize(w);
            });

        session->Send(std::move(bytes));
    }

    void YunoLoginServerNetwork::SendLoginRejected(
        std::shared_ptr<yuno::net::TcpSession> session,
        yuno::net::packets::AuthResultCode code,
        const char* reason)
    {
        if (!session)
            return;

        yuno::net::packets::S2C_AuthResult result{};
        result.success = 0;
        result.code = code;
        result.message = reason ? std::string(reason) : std::string("UNKNOWN");
        result.gamePort = 0;

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::S2C_AuthResult,
            [&result](yuno::net::ByteWriter& w)
            {
                result.Serialize(w);
            });

        session->Send(std::move(bytes));
    }
}
