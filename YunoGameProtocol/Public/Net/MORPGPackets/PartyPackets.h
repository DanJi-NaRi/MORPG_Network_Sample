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
    enum class PartyResultCode : std::uint16_t
    {
        None = 0,
        NotInWorld = 1,
        AlreadyInParty = 2,
        PartyNotFound = 3,
        PartyFull = 4,
        NotPartyLeader = 5,
    };

    struct PartyMemberState final
    {
        std::uint32_t entityId = 0;
        std::uint8_t online = 0;
        std::uint8_t alive = 0;
        std::string displayName;

        void Serialize(ByteWriter& w) const;
        static PartyMemberState Deserialize(ByteReader& r);
    };

    struct C2S_PartyCreate final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_PartyCreate Deserialize(ByteReader& r);
    };

    struct C2S_PartyJoin final
    {
        std::uint32_t partyId = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_PartyJoin Deserialize(ByteReader& r);
    };

    struct C2S_PartyLeave final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_PartyLeave Deserialize(ByteReader& r);
    };

    struct C2S_PartyList final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_PartyList Deserialize(ByteReader& r);
    };

    struct PartyListEntry final
    {
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::uint16_t memberCount = 0;
        std::string leaderName;

        void Serialize(ByteWriter& w) const;
        static PartyListEntry Deserialize(ByteReader& r);
    };

    struct S2C_PartyState final
    {
        PartyResultCode resultCode = PartyResultCode::None;
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::vector<PartyMemberState> members;

        void Serialize(ByteWriter& w) const;
        static S2C_PartyState Deserialize(ByteReader& r);
    };

    struct S2C_PartyList final
    {
        std::vector<PartyListEntry> parties;

        void Serialize(ByteWriter& w) const;
        static S2C_PartyList Deserialize(ByteReader& r);
    };
}
