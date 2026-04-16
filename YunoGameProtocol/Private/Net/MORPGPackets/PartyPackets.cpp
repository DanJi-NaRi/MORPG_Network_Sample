#include "pch.h"

#include "PartyPackets.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxPartyMembers = 8;
        constexpr std::uint16_t kMaxPartyListEntries = 32;

        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("PartyPackets string too long");

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
                throw std::runtime_error("PartyPackets invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }
            return out;
        }
    }

    void PartyMemberState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        w.WriteU8(online);
        w.WriteU8(alive);
        WriteStringU16(w, displayName);
    }

    PartyMemberState PartyMemberState::Deserialize(ByteReader& r)
    {
        PartyMemberState state{};
        state.entityId = r.ReadU32LE();
        state.online = r.ReadU8();
        state.alive = r.ReadU8();
        state.displayName = ReadStringU16(r);
        return state;
    }

    void C2S_PartyCreate::Serialize(ByteWriter& w) const
    {
        (void)w;
    }

    C2S_PartyCreate C2S_PartyCreate::Deserialize(ByteReader& r)
    {
        (void)r;
        return {};
    }

    void C2S_PartyJoin::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(partyId);
    }

    C2S_PartyJoin C2S_PartyJoin::Deserialize(ByteReader& r)
    {
        C2S_PartyJoin pkt{};
        pkt.partyId = r.ReadU32LE();
        return pkt;
    }

    void C2S_PartyLeave::Serialize(ByteWriter& w) const
    {
        (void)w;
    }

    C2S_PartyLeave C2S_PartyLeave::Deserialize(ByteReader& r)
    {
        (void)r;
        return {};
    }

    void C2S_PartyList::Serialize(ByteWriter& w) const
    {
        (void)w;
    }

    C2S_PartyList C2S_PartyList::Deserialize(ByteReader& r)
    {
        (void)r;
        return {};
    }

    void PartyListEntry::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(partyId);
        w.WriteU32LE(leaderEntityId);
        w.WriteU16LE(memberCount);
        WriteStringU16(w, leaderName);
    }

    PartyListEntry PartyListEntry::Deserialize(ByteReader& r)
    {
        PartyListEntry entry{};
        entry.partyId = r.ReadU32LE();
        entry.leaderEntityId = r.ReadU32LE();
        entry.memberCount = r.ReadU16LE();
        entry.leaderName = ReadStringU16(r);
        return entry;
    }

    void S2C_PartyState::Serialize(ByteWriter& w) const
    {
        w.WriteU16LE(static_cast<std::uint16_t>(resultCode));
        w.WriteU32LE(partyId);
        w.WriteU32LE(leaderEntityId);

        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(members.size(), kMaxPartyMembers));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            members[i].Serialize(w);
        }
    }

    S2C_PartyState S2C_PartyState::Deserialize(ByteReader& r)
    {
        S2C_PartyState pkt{};
        pkt.resultCode = static_cast<PartyResultCode>(r.ReadU16LE());
        pkt.partyId = r.ReadU32LE();
        pkt.leaderEntityId = r.ReadU32LE();

        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxPartyMembers)
            count = kMaxPartyMembers;

        pkt.members.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            pkt.members.push_back(PartyMemberState::Deserialize(r));
        }
        return pkt;
    }

    void S2C_PartyList::Serialize(ByteWriter& w) const
    {
        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(parties.size(), kMaxPartyListEntries));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            parties[i].Serialize(w);
        }
    }

    S2C_PartyList S2C_PartyList::Deserialize(ByteReader& r)
    {
        S2C_PartyList pkt{};
        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxPartyListEntries)
            count = kMaxPartyListEntries;

        pkt.parties.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            pkt.parties.push_back(PartyListEntry::Deserialize(r));
        }
        return pkt;
    }
}
