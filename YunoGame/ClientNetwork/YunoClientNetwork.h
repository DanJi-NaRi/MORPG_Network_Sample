#pragma once

#include <boost/asio.hpp>
#include <functional>

#include "TcpClient.h"          // YunoNetTransport
#include "PacketDispatcher.h"   // YunoNetProtocol
#include "NetPeer.h"            // YunoNetProtocol

class GameManager;

namespace yuno::game
{
    // 寃뚯엫 硫붿씤 <-> ?ㅽ듃?뚰겕 ?ㅻ젅???곌껐???섑띁
    class YunoClientNetwork final
    {
    public:
        YunoClientNetwork();
        ~YunoClientNetwork();

        YunoClientNetwork(const YunoClientNetwork&) = delete;
        YunoClientNetwork& operator=(const YunoClientNetwork&) = delete;

        void Start(const std::string& host, std::uint16_t port);
        void Stop();

        bool IsConnected() const;

        // ?쒕쾭?쒗뀒 諛쏆? ?⑦궥???붿뒪?⑥퀜濡??꾨떖
        void PumpIncoming(float dt);

        // 硫붿씤 ?ㅻ젅?쒖뿉???몄텧: "?꾩꽦 ?⑦궥(?ㅻ뜑+諛붾뵒)" 諛붿씠?몃? ?≪떊 ?붿껌
        void SendPacket(std::vector<std::uint8_t> packetBytes);

        // 寃뚯엫?먯꽌 ?몃뱾???깅줉?????덇쾶 dispatcher ?묎렐 ?쒓났
        yuno::net::PacketDispatcher& Dispatcher() { return m_dispatcher; }
        using RawPacketTapFn = std::function<void(const std::vector<std::uint8_t>& packetBytes)>;
        void SetRawPacketTap(RawPacketTapFn fn) { m_rawPacketTap = std::move(fn); }

        // ?몃뱾???깅줉
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

        // --- Main thread processing ---
        yuno::net::PacketDispatcher m_dispatcher{ yuno::net::PacketDispatcher::EndpointRole::Client };
        yuno::net::NetPeer m_serverPeer{}; // client???쒕쾭 peer瑜??섎굹濡?痍④툒 (sId=0?쇰줈 ?쒖옉)

        // --- Incoming queue (net thread -> main thread) ---
        std::mutex m_inMtx;
        std::deque<std::vector<std::uint8_t>> m_inQ;
        RawPacketTapFn m_rawPacketTap;


    };
}
