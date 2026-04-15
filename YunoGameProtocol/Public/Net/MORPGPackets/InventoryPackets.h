#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct InventoryItemState final
    {
        std::uint16_t slotNo = 0;
        std::uint32_t itemId = 0;
        std::uint16_t quantity = 0;
        std::string itemCode;

        void Serialize(ByteWriter& w) const;
        static InventoryItemState Deserialize(ByteReader& r);
    };

    struct S2C_InventoryState final
    {
        std::uint32_t characterId = 0;
        std::uint32_t gold = 0;
        std::vector<InventoryItemState> items;

        void Serialize(ByteWriter& w) const;
        static S2C_InventoryState Deserialize(ByteReader& r);
    };
}
