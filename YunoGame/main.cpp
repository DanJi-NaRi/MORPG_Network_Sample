#include "pch.h"

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "YunoEngine.h"
#include "GameApp.h"

#include <boost/asio.hpp>

#include "ByteIO.h"
#include "C2S_AuthHello.h"
#include "C2S_AuthLogout.h"
#include "C2S_AuthRegister.h"
#include "PacketBuilder.h"
#include "PacketHeader.h"
#include "PacketType.h"
#include "S2C_AuthResult.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
    std::string ReadEnvOrDefault(const char* name, const char* fallback)
    {
        if (!name || !(*name))
            return fallback ? std::string(fallback) : std::string();

        char* buf = nullptr;
        size_t len = 0;
        const errno_t ec = _dupenv_s(&buf, &len, name);
        if (ec != 0 || !buf)
            return fallback ? std::string(fallback) : std::string();

        std::string out(buf);
        free(buf);
        if (out.empty())
            return fallback ? std::string(fallback) : std::string();

        return out;
    }

    std::uint16_t ReadPortEnvOrDefault(const char* name, std::uint16_t fallback)
    {
        const std::string text = ReadEnvOrDefault(name, "");
        if (text.empty())
            return fallback;

        try
        {
            const unsigned long raw = std::stoul(text);
            if (raw >= 1 && raw <= 65535)
                return static_cast<std::uint16_t>(raw);
        }
        catch (...)
        {
        }

        return fallback;
    }

    bool PerformAuthRequest(
        const std::string& payloadA,
        const std::string& payloadB,
        yuno::net::PacketType requestType,
        yuno::net::packets::S2C_AuthResult& outResponse)
    {
        const std::string loginHost = ReadEnvOrDefault("YUNO_LOGIN_SERVER_HOST", "127.0.0.1");
        const std::uint16_t loginPort = ReadPortEnvOrDefault("YUNO_LOGIN_SERVER_PORT", 7000);

        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);
        boost::system::error_code ec;

        auto endpoints = resolver.resolve(loginHost, std::to_string(loginPort), ec);
        if (ec)
        {
            std::cout << u8"[로그인 서버 주소 해석 실패: " << ec.message() << "\n";
            return false;
        }

        boost::asio::connect(socket, endpoints, ec);
        if (ec)
        {
            std::cout << u8"[로그인 서버 접속 실패: " << ec.message() << "\n";
            return false;
        }

        std::vector<std::uint8_t> requestPacket;
        if (requestType == yuno::net::PacketType::C2S_AuthHello)
        {
            yuno::net::packets::C2S_AuthHello request{};
            request.loginId = payloadA;
            request.password = payloadB;
            requestPacket = yuno::net::PacketBuilder::Build(
                requestType,
                [&request](yuno::net::ByteWriter& w)
                {
                    request.Serialize(w);
                });
        }
        else if (requestType == yuno::net::PacketType::C2S_AuthRegister)
        {
            yuno::net::packets::C2S_AuthRegister request{};
            request.loginId = payloadA;
            request.password = payloadB;
            requestPacket = yuno::net::PacketBuilder::Build(
                requestType,
                [&request](yuno::net::ByteWriter& w)
                {
                    request.Serialize(w);
                });
        }
        else if (requestType == yuno::net::PacketType::C2S_AuthLogout)
        {
            yuno::net::packets::C2S_AuthLogout request{};
            request.token = payloadA;
            requestPacket = yuno::net::PacketBuilder::Build(
                requestType,
                [&request](yuno::net::ByteWriter& w)
                {
                    request.Serialize(w);
                });
        }
        else
        {
            std::cout << u8"[요청 타입 오류]\n";
            return false;
        }

        boost::asio::write(socket, boost::asio::buffer(requestPacket), ec);
        if (ec)
        {
            std::cout << u8"[로그인 요청 전송 실패: " << ec.message() << "\n";
            return false;
        }

        std::uint8_t headerBytes[yuno::net::yunoPacketHeaderSize] = {};
        boost::asio::read(socket, boost::asio::buffer(headerBytes, yuno::net::yunoPacketHeaderSize), ec);
        if (ec)
        {
            std::cout << u8"[로그인 응답 헤더 수신 실패: " << ec.message() << "\n";
            return false;
        }

        const yuno::net::PacketHeader packetHeader = yuno::net::UnPackHeaderLE(headerBytes);
        if (packetHeader.type != yuno::net::PacketType::S2C_AuthResult)
        {
            std::cout << u8"[로그인 응답 타입 오류]\n";
            return false;
        }

        std::vector<std::uint8_t> bodyBytes(packetHeader.bodyLength);
        if (!bodyBytes.empty())
        {
            boost::asio::read(socket, boost::asio::buffer(bodyBytes.data(), bodyBytes.size()), ec);
            if (ec)
            {
                std::cout << u8"[로그인 응답 바디 수신 실패: " << ec.message() << "\n";
                return false;
            }
        }

        try
        {
            yuno::net::ByteReader reader(bodyBytes.data(), bodyBytes.size());
            outResponse = yuno::net::packets::S2C_AuthResult::Deserialize(reader);
            if (reader.Remaining() != 0)
            {
                std::cout << u8"[로그인 응답 길이 오류]\n";
                return false;
            }
        }
        catch (const std::exception&)
        {
            std::cout << u8"[로그인 응답 역직렬화 실패]\n";
            return false;
        }

        return true;
    }

    bool PerformLoginHandshake(const std::string& loginId, const std::string& loginPw)
    {
        yuno::net::packets::S2C_AuthResult response{};
        if (!PerformAuthRequest(loginId, loginPw, yuno::net::PacketType::C2S_AuthHello, response))
            return false;

        if (response.success == 0)
        {
            std::cout << u8"[로그인 실패: " << response.message << "]\n";
            return false;
        }

        if (response.gameHost.empty() || response.gamePort == 0 || response.loginToken.empty())
        {
            std::cout << u8"[로그인 성공 응답 값 누락]\n";
            return false;
        }

        _putenv_s("YUNO_SERVER_HOST", response.gameHost.c_str());
        _putenv_s("YUNO_SERVER_PORT", std::to_string(response.gamePort).c_str());
        _putenv_s("YUNO_LOGIN_TOKEN", response.loginToken.c_str());

        std::cout << u8"[로그인 성공. 게임서버로 이동: "
                  << response.gameHost << ":" << response.gamePort << "\n";
        return true;
    }

    bool PerformRegister(const std::string& loginId, const std::string& loginPw)
    {
        yuno::net::packets::S2C_AuthResult response{};
        if (!PerformAuthRequest(loginId, loginPw, yuno::net::PacketType::C2S_AuthRegister, response))
            return false;

        if (response.success == 0)
        {
            std::cout << u8"[회원가입 실패: " << response.message << "]\n";
            return false;
        }

        std::cout << u8"[회원가입 성공]\n";
        return true;
    }

    void PerformLogoutHandshake()
    {
        const std::string token = ReadEnvOrDefault("YUNO_LOGIN_TOKEN", "");
        if (token.empty())
            return;

        yuno::net::packets::S2C_AuthResult response{};
        if (!PerformAuthRequest(token, "", yuno::net::PacketType::C2S_AuthLogout, response))
            return;

        if (response.success != 0)
            _putenv_s("YUNO_LOGIN_TOKEN", "");
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::cout << "[YunoGame] client startup\n";

#ifdef _DEBUG
    int dbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    dbgFlag |= _CRTDBG_ALLOC_MEM_DF;
    dbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(dbgFlag);
#endif

    while (true)
    {
        std::cout << "\n========== AUTH MENU ==========\n";
        std::cout << "1. Login\n";
        std::cout << "2. Register\n";
        std::cout << "Select (1/2): ";

        std::string menu;
        std::getline(std::cin, menu);

        if (menu == "1")
        {
            std::string loginId;
            std::string loginPw;
            std::cout << u8"로그인 ID 입력: ";
            std::getline(std::cin, loginId);
            std::cout << u8"로그인 PW 입력: ";
            std::getline(std::cin, loginPw);

            if (loginId.empty() || loginPw.empty())
            {
                std::cout << u8"[로그인 ID/PW를 모두 입력해야 합니다]\n";
                continue;
            }

            if (PerformLoginHandshake(loginId, loginPw))
                break;

            continue;
        }

        if (menu == "2")
        {
            std::string registerId;
            std::string registerPw;
            std::cout << u8"회원가입 ID 입력: ";
            std::getline(std::cin, registerId);
            std::cout << u8"회원가입 PW 입력: ";
            std::getline(std::cin, registerPw);

            if (registerId.empty() || registerPw.empty())
            {
                std::cout << u8"[회원가입 ID/PW를 모두 입력해야 합니다]\n";
                continue;
            }

            PerformRegister(registerId, registerPw);
            continue;
        }

        std::cout << u8"[1 또는 2를 입력하세요]\n";
    }

    GameApp game;
    YunoEngine engine;

    std::cout << "[CWD] " << std::filesystem::current_path().string() << "\n";
    if (!engine.Initialize(&game, L"YunoGame", 1920, 1080))
    {
        return -1;
    }

    const int runResult = engine.Run();
    PerformLogoutHandshake();
    return runResult;
}
