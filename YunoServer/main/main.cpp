#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "TcpClient.h"
#include "PacketBuilder.h"
#include "PacketHeader.h"
#include "PacketType.h"
#include "Net/C2SPackets/C2S_AuthHello.h"
#include "Net/C2SPackets/C2S_AuthRegister.h"
#include "Net/S2CPackets/S2C_AuthResult.h"
#include "C2S_AckSnapshot.h"
#include "C2S_EnterWorld.h"
#include "C2S_MoveInput.h"
#include "S2C_WorldSnapshot.h"
#include "YunoServerNetwork.h"

namespace
{
    struct BotRuntime
    {
        std::uint32_t id = 0;
        std::string username;
        std::string password;
        std::string token;

        std::unique_ptr<yuno::net::TcpClient> loginClient;
        std::unique_ptr<yuno::net::TcpClient> gameClient;

        bool loginRequested = false;
        bool loginAccepted = false;
        bool gameEnterRequested = false;
        bool inWorld = false;
        std::uint32_t entityId = 0;
        std::uint32_t moveClientTick = 0;
        std::uint32_t moveSequence = 0;
        float movePhase = 0.0f;
        std::uint32_t authFailures = 0;
        std::uint32_t gameFailures = 0;
    };

    std::uint16_t ParsePortOrDefault(const char* text, std::uint16_t fallback)
    {
        if (!text)
            return fallback;

        const int parsed = std::atoi(text);
        if (parsed > 0 && parsed <= 65535)
            return static_cast<std::uint16_t>(parsed);

        return fallback;
    }

    std::uint32_t ParseU32OrDefault(const char* text, std::uint32_t fallback)
    {
        if (!text)
            return fallback;

        const unsigned long parsed = std::strtoul(text, nullptr, 10);
        if (parsed > 0xFFFFFFFFUL)
            return fallback;
        return static_cast<std::uint32_t>(parsed);
    }

    void SendAuthRegister(yuno::net::TcpClient& client, const std::string& username, const std::string& password)
    {
        yuno::net::packets::C2S_AuthRegister reg{};
        reg.loginId = username;
        reg.password = password;
        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::C2S_AuthRegister,
            [&reg](yuno::net::ByteWriter& w)
            {
                reg.Serialize(w);
            });
        client.Send(std::move(bytes));
    }

    void SendAuthLogin(yuno::net::TcpClient& client, const std::string& username, const std::string& password)
    {
        yuno::net::packets::C2S_AuthHello hello{};
        hello.loginId = username;
        hello.password = password;
        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::C2S_AuthHello,
            [&hello](yuno::net::ByteWriter& w)
            {
                hello.Serialize(w);
            });
        client.Send(std::move(bytes));
    }

    void SendEnterWorld(yuno::net::TcpClient& client, const std::string& token)
    {
        yuno::net::packets::C2S_EnterWorld enterWorld{};
        enterWorld.characterId = 1001;
        enterWorld.spawnRegionId = 0;
        enterWorld.loginToken = token;
        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::C2S_EnterWorld,
            [&enterWorld](yuno::net::ByteWriter& w)
            {
                enterWorld.Serialize(w);
            });
        client.Send(std::move(bytes));
    }

    void SendMoveInput(yuno::net::TcpClient& client, BotRuntime& bot, float x, float y)
    {
        yuno::net::packets::C2S_MoveInput moveInput{};
        moveInput.entityId = bot.entityId;

        yuno::net::packets::MoveInputFrame frame{};
        frame.clientTick = ++bot.moveClientTick;
        frame.sequence = ++bot.moveSequence;
        frame.moveX = x;
        frame.moveY = y;
        frame.buttons = 0;
        moveInput.frames.push_back(frame);

        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::C2S_MoveInput,
            [&moveInput](yuno::net::ByteWriter& w)
            {
                moveInput.Serialize(w);
            });
        client.Send(std::move(bytes));
    }

    void SendSnapshotAck(yuno::net::TcpClient& client, std::uint32_t snapshotId)
    {
        yuno::net::packets::C2S_AckSnapshot ack{};
        ack.snapshotId = snapshotId;
        auto bytes = yuno::net::PacketBuilder::Build(
            yuno::net::PacketType::C2S_AckSnapshot,
            [&ack](yuno::net::ByteWriter& w)
            {
                ack.Serialize(w);
            });
        client.Send(std::move(bytes));
    }

    int RunBotMode(
        const std::string& loginHost,
        std::uint16_t loginPort,
        const std::string& gameHost,
        std::uint16_t gamePort,
        std::uint32_t botCount,
        std::uint32_t durationSec)
    {
        boost::asio::io_context io;
        std::vector<std::unique_ptr<BotRuntime>> bots;
        bots.reserve(botCount);

        for (std::uint32_t i = 0; i < botCount; ++i)
        {
            auto bot = std::make_unique<BotRuntime>();
            bot->id = i + 1;
            bot->username = "load_bot_" + std::to_string(i + 1);
            bot->password = "pw_" + std::to_string(i + 1);
            bot->movePhase = static_cast<float>(i) * 0.25f;
            bot->loginClient = std::make_unique<yuno::net::TcpClient>(io);
            bot->gameClient = std::make_unique<yuno::net::TcpClient>(io);

            BotRuntime* pBot = bot.get();

            pBot->loginClient->SetOnConnected([pBot]()
                {
                    SendAuthRegister(*pBot->loginClient, pBot->username, pBot->password);
                });

            pBot->loginClient->SetOnPacket([pBot, gameHost, gamePort](std::vector<std::uint8_t>&& packet)
                {
                    if (packet.size() < yuno::net::yunoPacketHeaderSize)
                        return;

                    const yuno::net::PacketHeader header = yuno::net::UnPackHeaderLE(packet.data());
                    if (header.type != yuno::net::PacketType::S2C_AuthResult)
                        return;

                    try
                    {
                        yuno::net::ByteReader reader(packet.data() + yuno::net::yunoPacketHeaderSize, header.bodyLength);
                        const auto result = yuno::net::packets::S2C_AuthResult::Deserialize(reader);
                        if (reader.Remaining() != 0)
                            return;

                        if (result.success != 0)
                        {
                            if (result.loginToken.empty())
                            {
                                if (!pBot->loginRequested)
                                {
                                    SendAuthLogin(*pBot->loginClient, pBot->username, pBot->password);
                                    pBot->loginRequested = true;
                                }
                                return;
                            }

                            pBot->token = result.loginToken;
                            pBot->loginAccepted = true;
                            if (!pBot->gameEnterRequested && !pBot->gameClient->IsConnected())
                            {
                                pBot->gameClient->Connect(gameHost, gamePort);
                            }
                            return;
                        }

                        if (result.code == yuno::net::packets::AuthResultCode::AccountExists && !pBot->loginRequested)
                        {
                            SendAuthLogin(*pBot->loginClient, pBot->username, pBot->password);
                            pBot->loginRequested = true;
                            return;
                        }

                        pBot->authFailures += 1;
                    }
                    catch (...)
                    {
                        pBot->authFailures += 1;
                    }
                });

            pBot->loginClient->SetOnDisconnected([pBot](const boost::system::error_code&)
                {
                    if (!pBot->loginAccepted)
                        pBot->authFailures += 1;
                });

            pBot->gameClient->SetOnConnected([pBot]()
                {
                    if (!pBot->token.empty())
                    {
                        SendEnterWorld(*pBot->gameClient, pBot->token);
                        pBot->gameEnterRequested = true;
                    }
                });

            pBot->gameClient->SetOnPacket([pBot](std::vector<std::uint8_t>&& packet)
                {
                    if (packet.size() < yuno::net::yunoPacketHeaderSize)
                        return;

                    const yuno::net::PacketHeader header = yuno::net::UnPackHeaderLE(packet.data());
                    try
                    {
                        yuno::net::ByteReader reader(packet.data() + yuno::net::yunoPacketHeaderSize, header.bodyLength);
                        if (header.type == yuno::net::PacketType::S2C_SpawnEntity)
                        {
                            const auto spawn = yuno::net::packets::S2C_SpawnEntity::Deserialize(reader);
                            if (reader.Remaining() != 0)
                                return;

                            if (pBot->entityId == 0)
                            {
                                pBot->entityId = spawn.entityId;
                                pBot->inWorld = true;
                            }
                            return;
                        }

                        if (header.type == yuno::net::PacketType::S2C_WorldSnapshot)
                        {
                            const auto snapshot = yuno::net::packets::S2C_WorldSnapshot::Deserialize(reader);
                            if (reader.Remaining() != 0)
                                return;
                            SendSnapshotAck(*pBot->gameClient, snapshot.snapshotId);
                            return;
                        }
                    }
                    catch (...)
                    {
                        pBot->gameFailures += 1;
                    }
                });

            pBot->gameClient->SetOnDisconnected([pBot](const boost::system::error_code&)
                {
                    if (pBot->gameEnterRequested && !pBot->inWorld)
                        pBot->gameFailures += 1;
                });

            pBot->loginClient->Connect(loginHost, loginPort);
            bots.push_back(std::move(bot));
        }

        std::cout << "[Bot] start. login=" << loginHost << ":" << loginPort
                  << " game=" << gameHost << ":" << gamePort
                  << " bots=" << botCount
                  << " durationSec=" << durationSec << "\n";

        const auto startTime = std::chrono::steady_clock::now();
        auto lastMoveSend = startTime;

        while (true)
        {
            while (io.poll_one() > 0)
            {
            }

            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() >= durationSec)
                break;

            if (now - lastMoveSend >= std::chrono::milliseconds(33))
            {
                lastMoveSend = now;
                for (auto& bot : bots)
                {
                    if (!bot->inWorld || bot->entityId == 0 || !bot->gameClient->IsConnected())
                        continue;

                    bot->movePhase += 0.17f;
                    const float moveX = std::cos(bot->movePhase);
                    const float moveY = std::sin(bot->movePhase);
                    SendMoveInput(*bot->gameClient, *bot, moveX, moveY);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::uint32_t loginOk = 0;
        std::uint32_t worldOk = 0;
        std::uint32_t authFailures = 0;
        std::uint32_t gameFailures = 0;
        for (auto& bot : bots)
        {
            if (bot->loginAccepted)
                loginOk += 1;
            if (bot->inWorld)
                worldOk += 1;
            authFailures += bot->authFailures;
            gameFailures += bot->gameFailures;

            bot->gameClient->Disconnect();
            bot->loginClient->Disconnect();
        }

        while (io.poll_one() > 0)
        {
        }

        std::cout << "[Bot] done. loginOk=" << loginOk << "/" << botCount
                  << " worldOk=" << worldOk << "/" << botCount
                  << " authFailures=" << authFailures
                  << " gameFailures=" << gameFailures << "\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--bot")
    {
        const std::string loginHost = (argc >= 3) ? argv[2] : "127.0.0.1";
        const std::uint16_t loginPort = (argc >= 4) ? ParsePortOrDefault(argv[3], 7000) : 7000;
        const std::string gameHost = (argc >= 5) ? argv[4] : "127.0.0.1";
        const std::uint16_t gamePort = (argc >= 6) ? ParsePortOrDefault(argv[5], 9000) : 9000;
        const std::uint32_t botCount = (argc >= 7) ? ParseU32OrDefault(argv[6], 50) : 50;
        const std::uint32_t durationSec = (argc >= 8) ? ParseU32OrDefault(argv[7], 30) : 30;

        return RunBotMode(loginHost, loginPort, gameHost, gamePort, botCount, durationSec);
    }

    std::uint16_t port = 9000;
    if (argc >= 2)
    {
        port = ParsePortOrDefault(argv[1], 9000);
    }

    yuno::server::YunoServerNetwork server;
    if (!server.Start(port))
        return 1;

    std::cout << "[YunoServer] realtime base server running. port=" << port << "\n";

    while (true)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
