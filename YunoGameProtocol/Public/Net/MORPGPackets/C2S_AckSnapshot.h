#pragma once

#include <cstdint>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct C2S_AckSnapshot final
    {
        std::uint32_t snapshotId = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_AckSnapshot Deserialize(ByteReader& r);
    };
}
