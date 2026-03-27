#include "pch.h"

#include "TcpSession.h"

namespace yuno::net
{
    TcpSession::TcpSession(sessionId sid, boost::asio::ip::tcp::socket socket)
        : m_sid(sid)
        , m_socket(std::move(socket))
        , m_strand(m_socket.get_executor())
        , m_idleTimer(m_socket.get_executor())
        , m_lastRecvTime(std::chrono::steady_clock::now())
    {
    }

    void TcpSession::Start()
    {
        boost::asio::dispatch(m_strand, [self = shared_from_this()]()
            {
                self->RefreshLastRecvTime();
                self->ArmIdleTimer();
                self->ReadHeader();
            });
    }

    void TcpSession::Close()
    {
        boost::asio::dispatch(m_strand, [self = shared_from_this()]()
            {
                self->m_disconnectedNotified = true;
                self->m_idleTimer.cancel();
                self->m_writeQ.clear();
                self->m_writeQueueBytes = 0;

                boost::system::error_code ec;
                self->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                self->m_socket.close(ec);
            });
    }

    void TcpSession::Send(std::vector<std::uint8_t> packetBytes)
    {
        auto shared = std::make_shared<const std::vector<std::uint8_t>>(std::move(packetBytes));
        Send(std::move(shared));
    }

    void TcpSession::Send(std::shared_ptr<const std::vector<std::uint8_t>> packetBytes)
    {
        boost::asio::dispatch(m_strand, [self = shared_from_this(), pkt = std::move(packetBytes)]() mutable
            {
                if (!pkt || pkt->size() < yunoTCPPacketHeaderSize)
                    return;

                if (!self->CheckWriteQueueBudget(pkt->size()))
                {
                    boost::system::error_code ec = boost::asio::error::no_buffer_space;
                    self->NotifyDisconnected(ec);
                    return;
                }

                const bool isEmpty = self->m_writeQ.empty();
                self->m_writeQueueBytes += pkt->size();
                self->m_writeQ.push_back(std::move(pkt));

                if (isEmpty && !self->m_writing)
                {
                    self->DoWrite();
                }
            });
    }

    void TcpSession::ReadHeader()
    {
        auto self = shared_from_this();

        boost::asio::async_read(
            m_socket,
            boost::asio::buffer(m_readHeader.data(), m_readHeader.size()),
            boost::asio::bind_executor(m_strand,
                [self](const boost::system::error_code& ec, std::size_t /*bytes*/)
                {
                    if (ec)
                    {
                        self->NotifyDisconnected(ec);
                        return;
                    }

                    const std::uint32_t bodyLen = TcpSession::ReadU32LE(self->m_readHeader.data());
                    if (bodyLen > kMaxBodyLengthBytes)
                    {
                        boost::system::error_code fake = boost::asio::error::message_size;
                        self->NotifyDisconnected(fake);
                        return;
                    }

                    self->RefreshLastRecvTime();
                    self->ReadBody(bodyLen);
                }
            )
        );
    }

    void TcpSession::ReadBody(std::uint32_t bodyLen)
    {
        auto self = shared_from_this();

        m_readBody.clear();
        m_readBody.resize(bodyLen);

        if (bodyLen == 0)
        {
            std::vector<std::uint8_t> packet;
            packet.reserve(yunoTCPPacketHeaderSize);
            packet.insert(packet.end(), m_readHeader.begin(), m_readHeader.end());

            if (self->m_onPacket)
                self->m_onPacket(std::move(packet));

            self->RefreshLastRecvTime();
            self->ReadHeader();
            return;
        }

        boost::asio::async_read(
            m_socket,
            boost::asio::buffer(m_readBody.data(), m_readBody.size()),
            boost::asio::bind_executor(m_strand,
                [self](const boost::system::error_code& ec, std::size_t /*bytes*/)
                {
                    if (ec)
                    {
                        self->NotifyDisconnected(ec);
                        return;
                    }

                    std::vector<std::uint8_t> packet;
                    packet.reserve(yunoTCPPacketHeaderSize + self->m_readBody.size());
                    packet.insert(packet.end(), self->m_readHeader.begin(), self->m_readHeader.end());
                    packet.insert(packet.end(), self->m_readBody.begin(), self->m_readBody.end());

                    if (self->m_onPacket)
                        self->m_onPacket(std::move(packet));

                    self->RefreshLastRecvTime();
                    self->ReadHeader();
                }
            )
        );
    }

    void TcpSession::DoWrite()
    {
        if (m_writeQ.empty())
        {
            m_writing = false;
            return;
        }

        m_writing = true;
        auto self = shared_from_this();
        auto pkt = m_writeQ.front();

        boost::asio::async_write(
            m_socket,
            boost::asio::buffer(pkt->data(), pkt->size()),
            boost::asio::bind_executor(m_strand,
                [self](const boost::system::error_code& ec, std::size_t /*bytes*/)
                {
                    if (ec)
                    {
                        self->NotifyDisconnected(ec);
                        return;
                    }

                    if (!self->m_writeQ.empty())
                    {
                        self->m_writeQueueBytes -= self->m_writeQ.front()->size();
                        self->m_writeQ.pop_front();
                    }
                    self->DoWrite();
                }
            )
        );
    }

    void TcpSession::NotifyDisconnected(const boost::system::error_code& ec)
    {
        if (m_disconnectedNotified)
            return;
        m_disconnectedNotified = true;

        if (m_onDisconnected)
            m_onDisconnected(ec);

        m_idleTimer.cancel();
        boost::system::error_code ignored;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        m_socket.close(ignored);
    }

    void TcpSession::ArmIdleTimer()
    {
        auto self = shared_from_this();
        m_idleTimer.expires_after(kIdleTimeout);
        m_idleTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec)
            {
                if (ec || self->m_disconnectedNotified)
                    return;

                const auto now = std::chrono::steady_clock::now();
                const auto idleFor = now - self->m_lastRecvTime;
                if (idleFor >= kIdleTimeout)
                {
                    boost::system::error_code timeoutEc = boost::asio::error::timed_out;
                    self->NotifyDisconnected(timeoutEc);
                    return;
                }

                self->ArmIdleTimer();
            }));
    }

    void TcpSession::RefreshLastRecvTime()
    {
        m_lastRecvTime = std::chrono::steady_clock::now();
    }

    bool TcpSession::CheckWriteQueueBudget(std::size_t nextPacketBytes) const
    {
        if (m_writeQ.size() >= kMaxWriteQueuePackets)
            return false;

        if (m_writeQueueBytes + nextPacketBytes > kMaxWriteQueueBytes)
            return false;

        return true;
    }

    std::uint32_t TcpSession::ReadU32LE(const std::uint8_t* p)
    {
        return (std::uint32_t)p[0]
            | ((std::uint32_t)p[1] << 8)
            | ((std::uint32_t)p[2] << 16)
            | ((std::uint32_t)p[3] << 24);
    }
}
