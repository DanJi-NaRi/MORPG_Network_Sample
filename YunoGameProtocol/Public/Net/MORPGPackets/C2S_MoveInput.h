#pragma once

#include <cstdint>
#include <vector>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct MoveInputFrame
    {
        std::uint32_t clientTick = 0;
        std::uint32_t sequence = 0;
        float moveX = 0.0f;
        float moveY = 0.0f;
        std::uint16_t buttons = 0;

        void Serialize(ByteWriter& w) const;
        static MoveInputFrame Deserialize(ByteReader& r);
    };

    struct C2S_MoveInput final
    {
        std::uint32_t entityId = 0;
        std::vector<MoveInputFrame> frames;

        void Serialize(ByteWriter& w) const;
        static C2S_MoveInput Deserialize(ByteReader& r);
    };
}
