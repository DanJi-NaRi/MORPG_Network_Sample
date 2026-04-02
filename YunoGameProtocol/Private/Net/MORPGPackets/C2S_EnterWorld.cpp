#include "pch.h"

#include "C2S_EnterWorld.h"
#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            const auto len = static_cast<std::uint16_t>(value.size());
            w.WriteU16LE(len);
            for (char c : value)
            {
                w.WriteU8(static_cast<std::uint8_t>(c));
            }
        }

        std::string ReadStringU16(ByteReader& r)
        {
            const std::uint16_t len = r.ReadU16LE();
            std::string out;
            out.resize(len);
            for (std::uint16_t i = 0; i < len; ++i)
            {
                out[i] = static_cast<char>(r.ReadU8());
            }
            return out;
        }
    }

    void C2S_EnterWorld::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(characterId);
        w.WriteU32LE(spawnRegionId);
        WriteStringU16(w, loginToken);
    }

    C2S_EnterWorld C2S_EnterWorld::Deserialize(ByteReader& r)
    {
        C2S_EnterWorld pkt{};
        pkt.characterId = r.ReadU32LE();
        pkt.spawnRegionId = r.ReadU32LE();
        pkt.loginToken = ReadStringU16(r);
        return pkt;
    }
}
