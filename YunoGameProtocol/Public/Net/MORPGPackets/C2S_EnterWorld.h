#pragma once

#include <cstdint>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct C2S_EnterWorld final
    {
        std::uint32_t characterId = 0;
        std::uint32_t spawnRegionId = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_EnterWorld Deserialize(ByteReader& r);
    };
}
