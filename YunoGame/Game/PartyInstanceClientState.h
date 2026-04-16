#pragma once

#include <algorithm>
#include <codecvt>
#include <cstdint>
#include <locale>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "InstancePackets.h"
#include "PartyPackets.h"

namespace yuno::game
{
    struct PartyListEntryView
    {
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::wstring leaderName;
        std::uint16_t memberCount = 0;
    };

    struct PartyMemberView
    {
        std::uint32_t entityId = 0;
        bool online = false;
        bool alive = false;
        std::wstring displayName;
    };

    struct HudSnapshot
    {
        std::vector<PartyListEntryView> availableParties;
        std::vector<PartyMemberView> partyMembers;
        std::unordered_map<std::uint32_t, std::wstring> displayNamesByEntityId;
        std::wstring statusText;
        std::wstring loadingText;
        std::wstring pendingInviteText;
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::uint32_t selectedPartyId = 0;
        std::uint32_t pendingInvitePartyId = 0;
        std::uint32_t instanceId = 0;
        bool hasParty = false;
        bool hasPendingInvite = false;
        bool instanceActive = false;
        bool loading = false;
    };

    struct PendingCommands
    {
        bool createParty = false;
        bool refreshPartyList = false;
        bool leaveParty = false;
        bool enterInstance = false;
        std::uint32_t joinPartyId = 0;
    };

    struct SharedPartyInstanceState
    {
        std::vector<PartyListEntryView> availableParties;
        std::vector<PartyMemberView> partyMembers;
        std::unordered_map<std::uint32_t, std::wstring> displayNamesByEntityId;
        std::wstring statusText;
        std::wstring loadingText;
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::uint32_t selectedPartyId = 0;
        std::uint32_t pendingInvitePartyId = 0;
        std::uint32_t instanceId = 0;
        bool hasParty = false;
        bool loading = false;
    };

    inline std::mutex g_partyInstanceStateMutex;
    inline SharedPartyInstanceState g_partyInstanceState{};
    inline PendingCommands g_pendingCommands{};

    inline std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();
        try
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            return converter.from_bytes(text);
        }
        catch (...)
        {
            return std::wstring(text.begin(), text.end());
        }
    }

    inline void SetStatusText(const std::wstring& text)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.statusText = text;
    }

    inline void SetLoadingState(bool loading, const std::wstring& text)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.loading = loading;
        g_partyInstanceState.loadingText = text;
    }

    inline void SelectParty(std::uint32_t partyId)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.selectedPartyId = partyId;
    }

    inline void QueueCreateParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.createParty = true;
        g_partyInstanceState.statusText = L"Party create requested.";
    }

    inline void QueueRefreshPartyList()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.refreshPartyList = true;
        g_partyInstanceState.statusText = L"Refreshing party list...";
    }

    inline void QueueJoinSelectedParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.joinPartyId = g_partyInstanceState.selectedPartyId;
        if (g_pendingCommands.joinPartyId != 0)
            g_partyInstanceState.statusText = L"Joining selected party...";
        else
            g_partyInstanceState.statusText = L"Select a party from the list first.";
    }

    inline void QueueInviteSelectedParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.pendingInvitePartyId = g_partyInstanceState.selectedPartyId;
        if (g_partyInstanceState.pendingInvitePartyId == 0)
        {
            g_partyInstanceState.statusText = L"Select a party first.";
            return;
        }

        auto it = std::find_if(
            g_partyInstanceState.availableParties.begin(),
            g_partyInstanceState.availableParties.end(),
            [](const PartyListEntryView& entry)
            {
                return entry.partyId == g_partyInstanceState.pendingInvitePartyId;
            });

        const std::wstring leaderName =
            (it != g_partyInstanceState.availableParties.end()) ? it->leaderName : L"Selected party";
        g_partyInstanceState.statusText = L"Invite ready for " + leaderName + L". Click Accept Invite to join.";
    }

    inline void QueueAcceptInvite()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        if (g_partyInstanceState.pendingInvitePartyId == 0)
        {
            g_partyInstanceState.statusText = L"No pending invite.";
            return;
        }

        g_pendingCommands.joinPartyId = g_partyInstanceState.pendingInvitePartyId;
        g_partyInstanceState.statusText = L"Accepting invite...";
    }

    inline void QueueLeaveParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.leaveParty = true;
        g_partyInstanceState.statusText = L"Leaving party...";
    }

    inline void QueueEnterInstance()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.enterInstance = true;
        g_partyInstanceState.loading = true;
        g_partyInstanceState.loadingText = L"Loading instance...";
        g_partyInstanceState.statusText = L"Instance entry requested.";
    }

    inline PendingCommands ConsumePendingCommands()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        PendingCommands copy = g_pendingCommands;
        g_pendingCommands = {};
        return copy;
    }

    inline void PublishPartyList(const yuno::net::packets::S2C_PartyList& packet)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.availableParties.clear();
        g_partyInstanceState.availableParties.reserve(packet.parties.size());
        for (const auto& party : packet.parties)
        {
            PartyListEntryView view{};
            view.partyId = party.partyId;
            view.leaderEntityId = party.leaderEntityId;
            view.leaderName = Utf8ToWide(party.leaderName);
            view.memberCount = party.memberCount;
            g_partyInstanceState.availableParties.push_back(view);
        }

        if (g_partyInstanceState.selectedPartyId != 0)
        {
            const auto it = std::find_if(
                g_partyInstanceState.availableParties.begin(),
                g_partyInstanceState.availableParties.end(),
                [](const PartyListEntryView& entry)
                {
                    return entry.partyId == g_partyInstanceState.selectedPartyId;
                });
            if (it == g_partyInstanceState.availableParties.end())
                g_partyInstanceState.selectedPartyId = 0;
        }

        g_partyInstanceState.statusText = g_partyInstanceState.availableParties.empty()
            ? L"No available parties found."
            : L"Party list refreshed.";
    }

    inline void PublishPartyState(const yuno::net::packets::S2C_PartyState& packet)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.partyId = packet.partyId;
        g_partyInstanceState.leaderEntityId = packet.leaderEntityId;
        g_partyInstanceState.hasParty = (packet.partyId != 0 && packet.resultCode == yuno::net::packets::PartyResultCode::None);
        g_partyInstanceState.partyMembers.clear();
        g_partyInstanceState.displayNamesByEntityId.clear();
        g_partyInstanceState.partyMembers.reserve(packet.members.size());

        for (const auto& member : packet.members)
        {
            PartyMemberView view{};
            view.entityId = member.entityId;
            view.online = member.online != 0;
            view.alive = member.alive != 0;
            view.displayName = Utf8ToWide(member.displayName);
            g_partyInstanceState.partyMembers.push_back(view);
            g_partyInstanceState.displayNamesByEntityId[member.entityId] = view.displayName;
        }

        if (!g_partyInstanceState.hasParty)
        {
            g_partyInstanceState.partyId = 0;
            g_partyInstanceState.leaderEntityId = 0;
            g_partyInstanceState.instanceId = 0;
            g_partyInstanceState.partyMembers.clear();
            g_partyInstanceState.displayNamesByEntityId.clear();
        }
        else
        {
            g_partyInstanceState.pendingInvitePartyId = 0;
        }

        switch (packet.resultCode)
        {
        case yuno::net::packets::PartyResultCode::None:
            g_partyInstanceState.statusText = g_partyInstanceState.hasParty
                ? L"Party updated."
                : L"Party cleared.";
            break;
        case yuno::net::packets::PartyResultCode::NotInWorld:
            g_partyInstanceState.statusText = L"Enter the world first.";
            break;
        case yuno::net::packets::PartyResultCode::AlreadyInParty:
            g_partyInstanceState.statusText = L"Already in a party.";
            break;
        case yuno::net::packets::PartyResultCode::PartyNotFound:
            g_partyInstanceState.statusText = L"Party not found.";
            break;
        case yuno::net::packets::PartyResultCode::PartyFull:
            g_partyInstanceState.statusText = L"Party is full.";
            break;
        case yuno::net::packets::PartyResultCode::NotPartyLeader:
            g_partyInstanceState.statusText = L"Leader only action.";
            break;
        default:
            g_partyInstanceState.statusText = L"Party update received.";
            break;
        }
    }

    inline void PublishInstanceState(const yuno::net::packets::S2C_InstanceState& packet)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.instanceId = packet.instanceId;

        switch (packet.resultCode)
        {
        case yuno::net::packets::InstanceResultCode::None:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = (packet.instanceId != 0)
                ? L"Instance active."
                : L"Instance state updated.";
            break;
        case yuno::net::packets::InstanceResultCode::NotInParty:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = L"Join a party first.";
            break;
        case yuno::net::packets::InstanceResultCode::NotPartyLeader:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = L"Only the leader can enter the instance.";
            break;
        case yuno::net::packets::InstanceResultCode::PartyNotReady:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = L"Party needs 3 ready players.";
            break;
        case yuno::net::packets::InstanceResultCode::AlreadyInInstance:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = L"Already in an instance.";
            break;
        default:
            g_partyInstanceState.loading = false;
            g_partyInstanceState.loadingText.clear();
            g_partyInstanceState.statusText = L"Instance request failed.";
            break;
        }
    }

    inline bool ConsumeHudSnapshot(HudSnapshot& out)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        out.availableParties = g_partyInstanceState.availableParties;
        out.partyMembers = g_partyInstanceState.partyMembers;
        out.displayNamesByEntityId = g_partyInstanceState.displayNamesByEntityId;
        out.statusText = g_partyInstanceState.statusText;
        out.loadingText = g_partyInstanceState.loadingText;
        out.partyId = g_partyInstanceState.partyId;
        out.leaderEntityId = g_partyInstanceState.leaderEntityId;
        out.selectedPartyId = g_partyInstanceState.selectedPartyId;
        out.pendingInvitePartyId = g_partyInstanceState.pendingInvitePartyId;
        out.instanceId = g_partyInstanceState.instanceId;
        out.hasParty = g_partyInstanceState.hasParty;
        out.hasPendingInvite = g_partyInstanceState.pendingInvitePartyId != 0;
        out.instanceActive = g_partyInstanceState.instanceId != 0;
        out.loading = g_partyInstanceState.loading;

        if (out.hasPendingInvite)
        {
            const auto it = std::find_if(
                out.availableParties.begin(),
                out.availableParties.end(),
                [&out](const PartyListEntryView& entry)
                {
                    return entry.partyId == out.pendingInvitePartyId;
                });
            const std::wstring leaderName = (it != out.availableParties.end()) ? it->leaderName : L"Selected party";
            out.pendingInviteText = L"Pending invite: " + leaderName;
        }
        else
        {
            out.pendingInviteText.clear();
        }

        return true;
    }
}
