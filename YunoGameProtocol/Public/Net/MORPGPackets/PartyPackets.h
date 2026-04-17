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
        AlreadyPending = 6,
        InviteNotFound = 7,
        JoinRequestNotFound = 8,
        TargetNotFound = 9,
        CannotTargetSelf = 10,
        TargetAlreadyInParty = 11,
        DuplicateInvite = 12,
        DuplicateJoinRequest = 13,
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

    struct ConnectedPlayerState final
    {
        std::uint32_t entityId = 0;
        std::string displayName;

        void Serialize(ByteWriter& w) const;
        static ConnectedPlayerState Deserialize(ByteReader& r);
    };

    struct PendingJoinRequestState final
    {
        std::uint32_t applicantEntityId = 0;
        std::string displayName;

        void Serialize(ByteWriter& w) const;
        static PendingJoinRequestState Deserialize(ByteReader& r);
    };

    struct IncomingInviteState final
    {
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::string leaderName;

        void Serialize(ByteWriter& w) const;
        static IncomingInviteState Deserialize(ByteReader& r);
    };

    struct C2S_PartyCreate final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_PartyCreate Deserialize(ByteReader& r);
    };

    // Legacy packet id retained for compatibility; semantics are now "apply to join".
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

    struct C2S_PartyInvitePlayer final
    {
        std::uint32_t targetEntityId = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_PartyInvitePlayer Deserialize(ByteReader& r);
    };

    struct C2S_PartyRespondJoinRequest final
    {
        std::uint32_t applicantEntityId = 0;
        std::uint8_t accept = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_PartyRespondJoinRequest Deserialize(ByteReader& r);
    };

    struct C2S_PartyRespondInvite final
    {
        std::uint32_t partyId = 0;
        std::uint8_t accept = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_PartyRespondInvite Deserialize(ByteReader& r);
    };

    struct C2S_PartyBrowsePlayers final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_PartyBrowsePlayers Deserialize(ByteReader& r);
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

    struct S2C_PartySocialState final
    {
        PartyResultCode resultCode = PartyResultCode::None;
        std::string statusText;
        std::vector<ConnectedPlayerState> connectedPlayers;
        std::vector<PendingJoinRequestState> joinRequests;
        std::vector<IncomingInviteState> incomingInvites;

        void Serialize(ByteWriter& w) const;
        static S2C_PartySocialState Deserialize(ByteReader& r);
    };
}
