#include "pch.h"

#include "C2S_Ping.h"


namespace yuno::net::packets
{
    void C2S_Ping::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(reqTime);
    }

    C2S_Ping C2S_Ping::Deserialize(ByteReader& r)
    {
        C2S_Ping out;
        out.reqTime = r.ReadU32LE();
        return out;
    }
}
