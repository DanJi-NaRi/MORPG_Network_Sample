#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "PartyPackets.h"

namespace yuno::server
{
    class PartyManager final
    {
    public:
        struct Party final
        {
            std::uint32_t partyId = 0;
            std::uint64_t leaderSessionId = 0;
            std::vector<std::uint64_t> members;
        };

        yuno::net::packets::PartyResultCode CreateParty(std::uint64_t sessionId, std::uint32_t& outPartyId);
        yuno::net::packets::PartyResultCode JoinParty(std::uint64_t sessionId, std::uint32_t partyId);
        yuno::net::packets::PartyResultCode RequestJoin(std::uint64_t sessionId, std::uint32_t partyId);
        yuno::net::packets::PartyResultCode InvitePlayer(std::uint64_t leaderSessionId, std::uint64_t targetSessionId);
        yuno::net::packets::PartyResultCode RespondToJoinRequest(std::uint64_t leaderSessionId, std::uint64_t applicantSessionId, bool accept, std::uint32_t& outPartyId);
        yuno::net::packets::PartyResultCode RespondToInvite(std::uint64_t sessionId, std::uint32_t partyId, bool accept, std::uint32_t& outJoinedPartyId);
        yuno::net::packets::PartyResultCode LeaveParty(std::uint64_t sessionId, std::uint32_t* outPartyId = nullptr);
        const Party* FindPartyById(std::uint32_t partyId) const;
        const Party* FindPartyByMember(std::uint64_t sessionId) const;
        std::vector<std::uint64_t> GetPartyMembers(std::uint32_t partyId) const;
        std::vector<std::uint64_t> GetPendingJoinRequests(std::uint32_t partyId) const;
        std::vector<std::uint32_t> GetIncomingInvites(std::uint64_t sessionId) const;
        std::vector<Party> ListParties() const;
        void RemoveDisconnected(std::uint64_t sessionId);

    private:
        static constexpr std::size_t kMaxPartyMembers = 3;

        yuno::net::packets::PartyResultCode AddMemberToParty(std::uint64_t sessionId, std::uint32_t partyId);
        bool IsPartyLeader(std::uint64_t sessionId, std::uint32_t* outPartyId = nullptr) const;
        void ClearSocialStateForSession(std::uint64_t sessionId);
        void ClearPartyAuxState(std::uint32_t partyId);
        void RemoveJoinRequestFromAll(std::uint64_t sessionId);
        void RemoveInviteFromAll(std::uint64_t sessionId);

        std::unordered_map<std::uint32_t, Party> m_parties;
        std::unordered_map<std::uint64_t, std::uint32_t> m_memberToParty;
        std::unordered_map<std::uint32_t, std::vector<std::uint64_t>> m_pendingJoinRequestsByParty;
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_incomingInvitesBySession;
        std::uint32_t m_nextPartyId = 1;
    };
}
