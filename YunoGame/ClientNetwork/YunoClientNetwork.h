#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <cstdint>
#include <functional>

#include "TcpClient.h"          // YunoNetTransport
#include "PacketDispatcher.h"   // YunoNetProtocol
#include "NetPeer.h"            // YunoNetProtocol

class GameManager;

namespace yuno::game
{
    // 野껊슣??筌롫뗄??<-> ??쎈뱜??곌쾿 ??살쟿???怨뚭퍙????묐쓠
    class YunoClientNetwork final
    {
    public:
        struct SnapshotAckDebugInfo
        {
            std::uint32_t lastReceivedSnapshotId = 0;
            std::uint32_t lastSentAckSnapshotId = 0;
            std::uint32_t lastServerSeenAckSnapshotId = 0;
        };

        YunoClientNetwork();
        ~YunoClientNetwork();

        YunoClientNetwork(const YunoClientNetwork&) = delete;
        YunoClientNetwork& operator=(const YunoClientNetwork&) = delete;

        void Start(const std::string& host, std::uint16_t port);
        void Stop();

        bool IsConnected() const;

        // ??뺤쒔??쀫?獄쏆룇? ???땅???遺용뮞??ν쒏에??袁⑤뼎
        void PumpIncoming(float dt);

        // 筌롫뗄????살쟿??뽯퓠???紐꾪뀱: "?袁⑷쉐 ???땅(??삳쐭+獄쏅뗀逾?" 獄쏅뗄??紐? ??る뻿 ?遺욧퍕
        void SendPacket(std::vector<std::uint8_t> packetBytes);
        SnapshotAckDebugInfo GetSnapshotAckDebugInfo() const;

        // 野껊슣??癒?퐣 ?紐껊굶???源낆쨯??????뉗쓺 dispatcher ?臾롫젏 ??볥궗
        yuno::net::PacketDispatcher& Dispatcher() { return m_dispatcher; }
        using RawPacketTapFn = std::function<void(const std::vector<std::uint8_t>& packetBytes)>;
        void SetRawPacketTap(RawPacketTapFn fn) { m_rawPacketTap = std::move(fn); }

        // ?紐껊굶???源낆쨯
    public:
        void RegisterMatchPacketHandler();

    private:
        void PushIncoming(std::vector<std::uint8_t>&& packetBytes);
        bool PopIncoming(std::vector<std::uint8_t>& out);

    private:
        // --- Network thread members ---
        boost::asio::io_context m_io;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
        std::thread m_netThread;

        yuno::net::TcpClient m_client;

        std::atomic<bool> m_running{ false };
        std::atomic<std::uint32_t> m_lastReceivedSnapshotId{ 0 };
        std::atomic<std::uint32_t> m_lastSentAckSnapshotId{ 0 };
        std::atomic<std::uint32_t> m_lastServerSeenAckSnapshotId{ 0 };

        // --- Main thread processing ---
        yuno::net::PacketDispatcher m_dispatcher{ yuno::net::PacketDispatcher::EndpointRole::Client };
        yuno::net::NetPeer m_serverPeer{}; // client????뺤쒔 peer????롪돌嚥??띯몿??(sId=0??곗쨮 ??뽰삂)

        // --- Incoming queue (net thread -> main thread) ---
        std::mutex m_inMtx;
        std::deque<std::vector<std::uint8_t>> m_inQ;
        RawPacketTapFn m_rawPacketTap;


    };
}
