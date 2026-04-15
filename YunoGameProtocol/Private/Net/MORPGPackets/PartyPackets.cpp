#include "pch.h"

#include "PartyPackets.h"

#include <algorithm>

#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxPartyMembers = 8;
    }

    void PartyMemberState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        w.WriteU8(online);
        w.WriteU8(alive);
    }

    PartyMemberState PartyMemberState::Deserialize(ByteReader& r)
    {
        PartyMemberState state{};
        state.entityId = r.ReadU32LE();
        state.online = r.ReadU8();
        state.alive = r.ReadU8();
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
}
