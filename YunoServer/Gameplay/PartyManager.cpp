#include "PartyManager.h"

#include <algorithm>

namespace yuno::server
{
    yuno::net::packets::PartyResultCode PartyManager::CreateParty(std::uint64_t sessionId, std::uint32_t& outPartyId)
    {
        outPartyId = 0;
        if (m_memberToParty.find(sessionId) != m_memberToParty.end())
            return yuno::net::packets::PartyResultCode::AlreadyInParty;

        Party party{};
        party.partyId = m_nextPartyId++;
        party.leaderSessionId = sessionId;
        party.members.push_back(sessionId);
        m_parties.emplace(party.partyId, party);
        m_memberToParty.emplace(sessionId, party.partyId);
        outPartyId = party.partyId;
        return yuno::net::packets::PartyResultCode::None;
    }

    yuno::net::packets::PartyResultCode PartyManager::JoinParty(std::uint64_t sessionId, std::uint32_t partyId)
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
        return yuno::net::packets::PartyResultCode::None;
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
            return yuno::net::packets::PartyResultCode::PartyNotFound;
        }

        Party& party = partyIt->second;
        party.members.erase(
            std::remove(party.members.begin(), party.members.end(), sessionId),
            party.members.end());
        m_memberToParty.erase(memberIt);

        if (party.members.empty())
        {
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

    void PartyManager::RemoveDisconnected(std::uint64_t sessionId)
    {
        (void)LeaveParty(sessionId, nullptr);
    }
}
