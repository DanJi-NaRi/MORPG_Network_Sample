#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <array>
#include <vector>

#include <boost/asio.hpp>

namespace yuno::net
{
    using sessionId = std::uint64_t;
}

namespace yuno::net
{
    inline constexpr std::size_t yunoTCPPacketHeaderSize = 8;

    class TcpSession final : public std::enable_shared_from_this<TcpSession>
    {
    public:
        using YunoSession = std::shared_ptr<TcpSession>;
        using OnPacketFn = std::function<void(std::vector<std::uint8_t>&& packet)>;
        using OnDisconnectedFn = std::function<void(const boost::system::error_code& ec)>;

    public:
        explicit TcpSession(sessionId sid, boost::asio::ip::tcp::socket socket);

        TcpSession(const TcpSession&) = delete;
        TcpSession& operator=(const TcpSession&) = delete;

        sessionId GetSessionId() const { return m_sid; }

        void Start();
        void Close();

        void Send(std::vector<std::uint8_t> packetBytes);
        void Send(std::shared_ptr<const std::vector<std::uint8_t>> packetBytes);

        void SetOnPacket(OnPacketFn fn) { m_onPacket = std::move(fn); }
        void SetOnDisconnected(OnDisconnectedFn fn) { m_onDisconnected = std::move(fn); }

        boost::asio::ip::tcp::socket& Socket() { return m_socket; }

    private:
        void ReadHeader();
        void ReadBody(std::uint32_t bodyLength);
        void DoWrite();

        void NotifyDisconnected(const boost::system::error_code& ec);
        void ArmIdleTimer();
        void RefreshLastRecvTime();
        bool CheckWriteQueueBudget(std::size_t nextPacketBytes) const;

        static std::uint32_t ReadU32LE(const std::uint8_t* p);

    private:
        sessionId m_sid = 0;

        boost::asio::ip::tcp::socket m_socket;
        boost::asio::strand<boost::asio::any_io_executor> m_strand;
        boost::asio::steady_timer m_idleTimer;
        std::chrono::steady_clock::time_point m_lastRecvTime{};

        std::array<std::uint8_t, yunoTCPPacketHeaderSize> m_readHeader{};
        std::vector<std::uint8_t> m_readBody;

        std::deque<std::shared_ptr<const std::vector<std::uint8_t>>> m_writeQ;
        std::size_t m_writeQueueBytes = 0;
        bool m_writing = false;

        OnPacketFn m_onPacket;
        OnDisconnectedFn m_onDisconnected;
        bool m_disconnectedNotified = false;

        static constexpr std::uint32_t kMaxBodyLengthBytes = 4u * 1024u * 1024u;
        static constexpr std::size_t kMaxWriteQueuePackets = 1024;
        static constexpr std::size_t kMaxWriteQueueBytes = 8u * 1024u * 1024u;
        static constexpr std::chrono::seconds kIdleTimeout{ 30 };
    };
}
