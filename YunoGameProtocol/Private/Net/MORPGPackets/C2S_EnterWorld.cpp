#include "pch.h"

#include "C2S_EnterWorld.h"
#include "ByteIO.h"

namespace yuno::net::packets
{
    void C2S_EnterWorld::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(characterId);
        w.WriteU32LE(spawnRegionId);
    }

    C2S_EnterWorld C2S_EnterWorld::Deserialize(ByteReader& r)
    {
        C2S_EnterWorld pkt{};
        pkt.characterId = r.ReadU32LE();
        pkt.spawnRegionId = r.ReadU32LE();
        return pkt;
    }
}
