#include "pch.h"

#include <algorithm>

#include "C2S_MoveInput.h"
#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxMoveFramesPerPacket = 64;
    }

    void MoveInputFrame::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(clientTick);
        w.WriteU32LE(sequence);
        w.WriteU16LE(static_cast<std::uint16_t>(moveX));
        w.WriteU16LE(static_cast<std::uint16_t>(moveY));
        w.WriteU16LE(buttons);
    }

    MoveInputFrame MoveInputFrame::Deserialize(ByteReader& r)
    {
        MoveInputFrame frame{};
        frame.clientTick = r.ReadU32LE();
        frame.sequence = r.ReadU32LE();
        frame.moveX = static_cast<std::int16_t>(r.ReadU16LE());
        frame.moveY = static_cast<std::int16_t>(r.ReadU16LE());
        frame.buttons = r.ReadU16LE();
        return frame;
    }

    void C2S_MoveInput::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(frames.size(), kMaxMoveFramesPerPacket));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            frames[i].Serialize(w);
        }
    }

    C2S_MoveInput C2S_MoveInput::Deserialize(ByteReader& r)
    {
        C2S_MoveInput pkt{};
        pkt.entityId = r.ReadU32LE();

        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxMoveFramesPerPacket)
            count = kMaxMoveFramesPerPacket;

        pkt.frames.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            pkt.frames.push_back(MoveInputFrame::Deserialize(r));
        }

        return pkt;
    }
}
