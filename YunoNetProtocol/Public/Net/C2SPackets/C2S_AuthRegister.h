#pragma once

#include <string>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct C2S_AuthRegister final
    {
        std::string loginId;
        std::string password;

        void Serialize(ByteWriter& w) const;
        static C2S_AuthRegister Deserialize(ByteReader& r);
    };
}
