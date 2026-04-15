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
        yuno::net::packets::PartyResultCode LeaveParty(std::uint64_t sessionId, std::uint32_t* outPartyId = nullptr);
        const Party* FindPartyById(std::uint32_t partyId) const;
        const Party* FindPartyByMember(std::uint64_t sessionId) const;
        std::vector<std::uint64_t> GetPartyMembers(std::uint32_t partyId) const;
        void RemoveDisconnected(std::uint64_t sessionId);

    private:
        static constexpr std::size_t kMaxPartyMembers = 3;

        std::unordered_map<std::uint32_t, Party> m_parties;
        std::unordered_map<std::uint64_t, std::uint32_t> m_memberToParty;
        std::uint32_t m_nextPartyId = 1;
    };
}
