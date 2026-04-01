#pragma once

#include <string>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct C2S_AuthLogout final
    {
        std::string token;

        void Serialize(ByteWriter& w) const;
        static C2S_AuthLogout Deserialize(ByteReader& r);
    };
}
