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
        constexpr std::uint16_t kMaxSocialEntries = 64;

        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("PartyPackets string too long");

            w.WriteU16LE(static_cast<std::uint16_t>(value.size()));
            for (char ch : value)
                w.WriteU8(static_cast<std::uint8_t>(ch));
        }

        std::string ReadStringU16(ByteReader& r)
        {
            const std::uint16_t length = r.ReadU16LE();
            if (!r.Has(length))
                throw std::runtime_error("PartyPackets invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
                out.push_back(static_cast<char>(r.ReadU8()));
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

    void ConnectedPlayerState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        WriteStringU16(w, displayName);
    }

    ConnectedPlayerState ConnectedPlayerState::Deserialize(ByteReader& r)
    {
        ConnectedPlayerState state{};
        state.entityId = r.ReadU32LE();
        state.displayName = ReadStringU16(r);
        return state;
    }

    void PendingJoinRequestState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(applicantEntityId);
        WriteStringU16(w, displayName);
    }

    PendingJoinRequestState PendingJoinRequestState::Deserialize(ByteReader& r)
    {
        PendingJoinRequestState state{};
        state.applicantEntityId = r.ReadU32LE();
        state.displayName = ReadStringU16(r);
        return state;
    }

    void IncomingInviteState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(partyId);
        w.WriteU32LE(leaderEntityId);
        WriteStringU16(w, leaderName);
    }

    IncomingInviteState IncomingInviteState::Deserialize(ByteReader& r)
    {
        IncomingInviteState state{};
        state.partyId = r.ReadU32LE();
        state.leaderEntityId = r.ReadU32LE();
        state.leaderName = ReadStringU16(r);
        return state;
    }

    void C2S_PartyCreate::Serialize(ByteWriter& w) const { (void)w; }
    C2S_PartyCreate C2S_PartyCreate::Deserialize(ByteReader& r) { (void)r; return {}; }

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

    void C2S_PartyLeave::Serialize(ByteWriter& w) const { (void)w; }
    C2S_PartyLeave C2S_PartyLeave::Deserialize(ByteReader& r) { (void)r; return {}; }

    void C2S_PartyList::Serialize(ByteWriter& w) const { (void)w; }
    C2S_PartyList C2S_PartyList::Deserialize(ByteReader& r) { (void)r; return {}; }

    void C2S_PartyInvitePlayer::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(targetEntityId);
    }

    C2S_PartyInvitePlayer C2S_PartyInvitePlayer::Deserialize(ByteReader& r)
    {
        C2S_PartyInvitePlayer pkt{};
        pkt.targetEntityId = r.ReadU32LE();
        return pkt;
    }

    void C2S_PartyRespondJoinRequest::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(applicantEntityId);
        w.WriteU8(accept);
    }

    C2S_PartyRespondJoinRequest C2S_PartyRespondJoinRequest::Deserialize(ByteReader& r)
    {
        C2S_PartyRespondJoinRequest pkt{};
        pkt.applicantEntityId = r.ReadU32LE();
        pkt.accept = r.ReadU8();
        return pkt;
    }

    void C2S_PartyRespondInvite::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(partyId);
        w.WriteU8(accept);
    }

    C2S_PartyRespondInvite C2S_PartyRespondInvite::Deserialize(ByteReader& r)
    {
        C2S_PartyRespondInvite pkt{};
        pkt.partyId = r.ReadU32LE();
        pkt.accept = r.ReadU8();
        return pkt;
    }

    void C2S_PartyBrowsePlayers::Serialize(ByteWriter& w) const { (void)w; }
    C2S_PartyBrowsePlayers C2S_PartyBrowsePlayers::Deserialize(ByteReader& r) { (void)r; return {}; }

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
            members[i].Serialize(w);
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
            pkt.members.push_back(PartyMemberState::Deserialize(r));
        return pkt;
    }

    void S2C_PartyList::Serialize(ByteWriter& w) const
    {
        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(parties.size(), kMaxPartyListEntries));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
            parties[i].Serialize(w);
    }

    S2C_PartyList S2C_PartyList::Deserialize(ByteReader& r)
    {
        S2C_PartyList pkt{};
        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxPartyListEntries)
            count = kMaxPartyListEntries;

        pkt.parties.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
            pkt.parties.push_back(PartyListEntry::Deserialize(r));
        return pkt;
    }

    void S2C_PartySocialState::Serialize(ByteWriter& w) const
    {
        w.WriteU16LE(static_cast<std::uint16_t>(resultCode));
        WriteStringU16(w, statusText);

        const std::uint16_t connectedCount = static_cast<std::uint16_t>(std::min<std::size_t>(connectedPlayers.size(), kMaxSocialEntries));
        w.WriteU16LE(connectedCount);
        for (std::uint16_t i = 0; i < connectedCount; ++i)
            connectedPlayers[i].Serialize(w);

        const std::uint16_t requestCount = static_cast<std::uint16_t>(std::min<std::size_t>(joinRequests.size(), kMaxSocialEntries));
        w.WriteU16LE(requestCount);
        for (std::uint16_t i = 0; i < requestCount; ++i)
            joinRequests[i].Serialize(w);

        const std::uint16_t inviteCount = static_cast<std::uint16_t>(std::min<std::size_t>(incomingInvites.size(), kMaxSocialEntries));
        w.WriteU16LE(inviteCount);
        for (std::uint16_t i = 0; i < inviteCount; ++i)
            incomingInvites[i].Serialize(w);
    }

    S2C_PartySocialState S2C_PartySocialState::Deserialize(ByteReader& r)
    {
        S2C_PartySocialState pkt{};
        pkt.resultCode = static_cast<PartyResultCode>(r.ReadU16LE());
        pkt.statusText = ReadStringU16(r);

        std::uint16_t connectedCount = r.ReadU16LE();
        if (connectedCount > kMaxSocialEntries)
            connectedCount = kMaxSocialEntries;
        pkt.connectedPlayers.reserve(connectedCount);
        for (std::uint16_t i = 0; i < connectedCount; ++i)
            pkt.connectedPlayers.push_back(ConnectedPlayerState::Deserialize(r));

        std::uint16_t requestCount = r.ReadU16LE();
        if (requestCount > kMaxSocialEntries)
            requestCount = kMaxSocialEntries;
        pkt.joinRequests.reserve(requestCount);
        for (std::uint16_t i = 0; i < requestCount; ++i)
            pkt.joinRequests.push_back(PendingJoinRequestState::Deserialize(r));

        std::uint16_t inviteCount = r.ReadU16LE();
        if (inviteCount > kMaxSocialEntries)
            inviteCount = kMaxSocialEntries;
        pkt.incomingInvites.reserve(inviteCount);
        for (std::uint16_t i = 0; i < inviteCount; ++i)
            pkt.incomingInvites.push_back(IncomingInviteState::Deserialize(r));

        return pkt;
    }
}
