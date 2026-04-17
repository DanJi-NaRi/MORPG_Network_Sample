#include "PartyManager.h"

#include <algorithm>

namespace yuno::server
{
    namespace
    {
        template <typename T>
        bool Contains(const std::vector<T>& values, const T& needle)
        {
            return std::find(values.begin(), values.end(), needle) != values.end();
        }
    }

    yuno::net::packets::PartyResultCode PartyManager::CreateParty(std::uint64_t sessionId, std::uint32_t& outPartyId)
    {
        outPartyId = 0;
        if (m_memberToParty.find(sessionId) != m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::AlreadyInParty;

        ClearSocialStateForSession(sessionId);

        Party party{};
        party.partyId = m_nextPartyId++;
        party.leaderSessionId = sessionId;
        party.members.push_back(sessionId);
        m_parties.emplace(party.partyId, party);
        m_memberToParty.emplace(sessionId, party.partyId);
        outPartyId = party.partyId;
        return yuno::net::packets::PartyResultCode::None;
    }

    yuno::net::packets::PartyResultCode PartyManager::AddMemberToParty(std::uint64_t sessionId, std::uint32_t partyId)
    {
        if (m_memberToParty.find(sessionId) != m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::AlreadyInParty;

        auto it = m_parties.find(partyId);
        if (it == m_parties.end())
            return yuno::net::packets::PartyResultCode::PartyNotFound;

        Party& party = it->second;
        if (party.members.size() >= kMaxPartyMembers)
            return yuno::net::packets::PartyResultCode::PartyFull;

        party.members.push_back(sessionId);
        m_memberToParty.emplace(sessionId, partyId);
        ClearSocialStateForSession(sessionId);
        return yuno::net::packets::PartyResultCode::None;
    }

    yuno::net::packets::PartyResultCode PartyManager::JoinParty(std::uint64_t sessionId, std::uint32_t partyId)
    {
        return AddMemberToParty(sessionId, partyId);
    }

    yuno::net::packets::PartyResultCode PartyManager::RequestJoin(std::uint64_t sessionId, std::uint32_t partyId)
    {
        if (m_memberToParty.find(sessionId) != m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::AlreadyInParty;

        auto partyIt = m_parties.find(partyId);
        if (partyIt == m_parties.end())
            return yuno::net::packets::PartyResultCode::PartyNotFound;

        if (partyIt->second.members.size() >= kMaxPartyMembers)
            return yuno::net::packets::PartyResultCode::PartyFull;

        auto& requests = m_pendingJoinRequestsByParty[partyId];
        if (Contains(requests, sessionId))
            return yuno::net::packets::PartyResultCode::DuplicateJoinRequest;

        requests.push_back(sessionId);
        return yuno::net::packets::PartyResultCode::None;
    }

    bool PartyManager::IsPartyLeader(std::uint64_t sessionId, std::uint32_t* outPartyId) const
    {
        const Party* party = FindPartyByMember(sessionId);
        if (!party || party->leaderSessionId != sessionId)
            return false;

        if (outPartyId)
            *outPartyId = party->partyId;
        return true;
    }

    yuno::net::packets::PartyResultCode PartyManager::InvitePlayer(std::uint64_t leaderSessionId, std::uint64_t targetSessionId)
    {
        if (leaderSessionId == targetSessionId)
            return yuno::net::packets::PartyResultCode::CannotTargetSelf;

        std::uint32_t partyId = 0;
        if (!IsPartyLeader(leaderSessionId, &partyId))
            return yuno::net::packets::PartyResultCode::NotPartyLeader;

        if (m_memberToParty.find(targetSessionId) != m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::TargetAlreadyInParty;

        const Party* party = FindPartyById(partyId);
        if (!party)
            return yuno::net::packets::PartyResultCode::PartyNotFound;
        if (party->members.size() >= kMaxPartyMembers)
            return yuno::net::packets::PartyResultCode::PartyFull;

        auto& invites = m_incomingInvitesBySession[targetSessionId];
        if (Contains(invites, partyId))
            return yuno::net::packets::PartyResultCode::DuplicateInvite;

        invites.push_back(partyId);
        return yuno::net::packets::PartyResultCode::None;
    }

    yuno::net::packets::PartyResultCode PartyManager::RespondToJoinRequest(
        std::uint64_t leaderSessionId,
        std::uint64_t applicantSessionId,
        bool accept,
        std::uint32_t& outPartyId)
    {
        outPartyId = 0;
        std::uint32_t partyId = 0;
        if (!IsPartyLeader(leaderSessionId, &partyId))
            return yuno::net::packets::PartyResultCode::NotPartyLeader;

        auto requestsIt = m_pendingJoinRequestsByParty.find(partyId);
        if (requestsIt == m_pendingJoinRequestsByParty.end())
            return yuno::net::packets::PartyResultCode::JoinRequestNotFound;

        auto& requests = requestsIt->second;
        auto requestIt = std::find(requests.begin(), requests.end(), applicantSessionId);
        if (requestIt == requests.end())
            return yuno::net::packets::PartyResultCode::JoinRequestNotFound;

        requests.erase(requestIt);
        if (requests.empty())
            m_pendingJoinRequestsByParty.erase(requestsIt);

        if (!accept)
            return yuno::net::packets::PartyResultCode::None;

        const auto code = AddMemberToParty(applicantSessionId, partyId);
        if (code == yuno::net::packets::PartyResultCode::None)
            outPartyId = partyId;
        return code;
    }

    yuno::net::packets::PartyResultCode PartyManager::RespondToInvite(
        std::uint64_t sessionId,
        std::uint32_t partyId,
        bool accept,
        std::uint32_t& outJoinedPartyId)
    {
        outJoinedPartyId = 0;
        auto invitesIt = m_incomingInvitesBySession.find(sessionId);
        if (invitesIt == m_incomingInvitesBySession.end())
            return yuno::net::packets::PartyResultCode::InviteNotFound;

        auto& invites = invitesIt->second;
        auto inviteIt = std::find(invites.begin(), invites.end(), partyId);
        if (inviteIt == invites.end())
            return yuno::net::packets::PartyResultCode::InviteNotFound;

        invites.erase(inviteIt);
        if (invites.empty())
            m_incomingInvitesBySession.erase(invitesIt);

        if (!accept)
            return yuno::net::packets::PartyResultCode::None;

        const auto code = AddMemberToParty(sessionId, partyId);
        if (code == yuno::net::packets::PartyResultCode::None)
            outJoinedPartyId = partyId;
        return code;
    }

    yuno::net::packets::PartyResultCode PartyManager::LeaveParty(std::uint64_t sessionId, std::uint32_t* outPartyId)
    {
        auto memberIt = m_memberToParty.find(sessionId);
        if (memberIt == m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::PartyNotFound;

        const std::uint32_t partyId = memberIt->second;
        if (outPartyId)
            *outPartyId = partyId;

        auto partyIt = m_parties.find(partyId);
        if (partyIt == m_parties.end())
        {
            m_memberToParty.erase(memberIt);
            ClearSocialStateForSession(sessionId);
            return yuno::net::packets::PartyResultCode::PartyNotFound;
        }

        Party& party = partyIt->second;
        party.members.erase(
            std::remove(party.members.begin(), party.members.end(), sessionId),
            party.members.end());
        m_memberToParty.erase(memberIt);
        ClearSocialStateForSession(sessionId);

        if (party.members.empty())
        {
            ClearPartyAuxState(partyId);
            m_parties.erase(partyIt);
            return yuno::net::packets::PartyResultCode::None;
        }

        if (party.leaderSessionId == sessionId)
            party.leaderSessionId = party.members.front();

        return yuno::net::packets::PartyResultCode::None;
    }

    const PartyManager::Party* PartyManager::FindPartyById(std::uint32_t partyId) const
    {
        auto it = m_parties.find(partyId);
        return (it != m_parties.end()) ? &it->second : nullptr;
    }

    const PartyManager::Party* PartyManager::FindPartyByMember(std::uint64_t sessionId) const
    {
        auto it = m_memberToParty.find(sessionId);
        if (it == m_memberToParty.end())
            return nullptr;
        return FindPartyById(it->second);
    }

    std::vector<std::uint64_t> PartyManager::GetPartyMembers(std::uint32_t partyId) const
    {
        const Party* party = FindPartyById(partyId);
        return party ? party->members : std::vector<std::uint64_t>{};
    }

    std::vector<std::uint64_t> PartyManager::GetPendingJoinRequests(std::uint32_t partyId) const
    {
        auto it = m_pendingJoinRequestsByParty.find(partyId);
        return (it != m_pendingJoinRequestsByParty.end()) ? it->second : std::vector<std::uint64_t>{};
    }

    std::vector<std::uint32_t> PartyManager::GetIncomingInvites(std::uint64_t sessionId) const
    {
        auto it = m_incomingInvitesBySession.find(sessionId);
        return (it != m_incomingInvitesBySession.end()) ? it->second : std::vector<std::uint32_t>{};
    }

    std::vector<PartyManager::Party> PartyManager::ListParties() const
    {
        std::vector<Party> parties;
        parties.reserve(m_parties.size());
        for (const auto& [partyId, party] : m_parties)
        {
            (void)partyId;
            parties.push_back(party);
        }
        std::sort(
            parties.begin(),
            parties.end(),
            [](const Party& lhs, const Party& rhs)
            {
                return lhs.partyId < rhs.partyId;
            });
        return parties;
    }

    void PartyManager::RemoveJoinRequestFromAll(std::uint64_t sessionId)
    {
        for (auto it = m_pendingJoinRequestsByParty.begin(); it != m_pendingJoinRequestsByParty.end();)
        {
            auto& requests = it->second;
            requests.erase(std::remove(requests.begin(), requests.end(), sessionId), requests.end());
            if (requests.empty())
                it = m_pendingJoinRequestsByParty.erase(it);
            else
                ++it;
        }
    }

    void PartyManager::RemoveInviteFromAll(std::uint64_t sessionId)
    {
        m_incomingInvitesBySession.erase(sessionId);
    }

    void PartyManager::ClearSocialStateForSession(std::uint64_t sessionId)
    {
        RemoveJoinRequestFromAll(sessionId);
        RemoveInviteFromAll(sessionId);
    }

    void PartyManager::ClearPartyAuxState(std::uint32_t partyId)
    {
        m_pendingJoinRequestsByParty.erase(partyId);
        for (auto it = m_incomingInvitesBySession.begin(); it != m_incomingInvitesBySession.end();)
        {
            auto& invites = it->second;
            invites.erase(std::remove(invites.begin(), invites.end(), partyId), invites.end());
            if (invites.empty())
                it = m_incomingInvitesBySession.erase(it);
            else
                ++it;
        }
    }

    void PartyManager::RemoveDisconnected(std::uint64_t sessionId)
    {
        ClearSocialStateForSession(sessionId);
        (void)LeaveParty(sessionId, nullptr);
    }
}
