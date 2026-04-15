#include "pch.h"

#include "InventoryPackets.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxInventoryItems = 128;

        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("S2C_InventoryState string too long");

            w.WriteU16LE(static_cast<std::uint16_t>(value.size()));
            for (char ch : value)
            {
                w.WriteU8(static_cast<std::uint8_t>(ch));
            }
        }

        std::string ReadStringU16(ByteReader& r)
        {
            const std::uint16_t length = r.ReadU16LE();
            if (!r.Has(length))
                throw std::runtime_error("S2C_InventoryState invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }

            return out;
        }
    }

    void InventoryItemState::Serialize(ByteWriter& w) const
    {
        w.WriteU16LE(slotNo);
        w.WriteU32LE(itemId);
        w.WriteU16LE(quantity);
        WriteStringU16(w, itemCode);
    }

    InventoryItemState InventoryItemState::Deserialize(ByteReader& r)
    {
        InventoryItemState item{};
        item.slotNo = r.ReadU16LE();
        item.itemId = r.ReadU32LE();
        item.quantity = r.ReadU16LE();
        item.itemCode = ReadStringU16(r);
        return item;
    }

    void S2C_InventoryState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(characterId);
        w.WriteU32LE(gold);

        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(items.size(), kMaxInventoryItems));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            items[i].Serialize(w);
        }
    }

    S2C_InventoryState S2C_InventoryState::Deserialize(ByteReader& r)
    {
        S2C_InventoryState pkt{};
        pkt.characterId = r.ReadU32LE();
        pkt.gold = r.ReadU32LE();

        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxInventoryItems)
            count = kMaxInventoryItems;

        pkt.items.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            pkt.items.push_back(InventoryItemState::Deserialize(r));
        }
        return pkt;
    }
}
