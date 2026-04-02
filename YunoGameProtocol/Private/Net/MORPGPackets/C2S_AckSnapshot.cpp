#include "pch.h"

#include "C2S_AckSnapshot.h"
#include "ByteIO.h"

namespace yuno::net::packets
{
    void C2S_AckSnapshot::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(snapshotId);
    }

    C2S_AckSnapshot C2S_AckSnapshot::Deserialize(ByteReader& r)
    {
        C2S_AckSnapshot pkt{};
        pkt.snapshotId = r.ReadU32LE();
        return pkt;
    }
}
