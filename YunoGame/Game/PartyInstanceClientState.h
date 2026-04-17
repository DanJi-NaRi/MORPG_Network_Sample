#pragma once

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "InstancePackets.h"
#include "PartyPackets.h"

namespace yuno::game
{
    enum class PartyPopupTab : std::uint8_t
    {
        InvitePlayers = 0,
        JoinRequests = 1,
        IncomingInvites = 2,
    };

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

    struct ConnectedPlayerView
    {
        std::uint32_t entityId = 0;
        std::wstring displayName;
    };

    struct JoinRequestView
    {
        std::uint32_t applicantEntityId = 0;
        std::wstring displayName;
    };

    struct IncomingInviteView
    {
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::wstring leaderName;
    };

    struct HudSnapshot
    {
        std::vector<PartyListEntryView> availableParties;
        std::vector<PartyMemberView> partyMembers;
        std::vector<ConnectedPlayerView> connectedPlayers;
        std::vector<JoinRequestView> joinRequests;
        std::vector<IncomingInviteView> incomingInvites;
        std::unordered_map<std::uint32_t, std::wstring> displayNamesByEntityId;
        std::wstring statusText;
        std::wstring loadingText;
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::uint32_t selectedPartyId = 0;
        std::uint32_t selectedConnectedPlayerEntityId = 0;
        std::uint32_t selectedJoinRequestEntityId = 0;
        std::uint32_t selectedIncomingInvitePartyId = 0;
        std::uint32_t instanceId = 0;
        bool hasParty = false;
        bool instanceActive = false;
        bool loading = false;
        bool partyPopupVisible = false;
        PartyPopupTab activePopupTab = PartyPopupTab::InvitePlayers;
    };

    struct PendingCommands
    {
        bool createParty = false;
        bool refreshPartyList = false;
        bool browsePartySocial = false;
        bool leaveParty = false;
        bool enterInstance = false;
        std::uint32_t applyPartyId = 0;
        std::uint32_t inviteTargetEntityId = 0;
        std::uint32_t respondJoinRequestEntityId = 0;
        bool respondJoinRequestAccepted = false;
        bool hasJoinRequestResponse = false;
        std::uint32_t respondInvitePartyId = 0;
        bool respondInviteAccepted = false;
        bool hasInviteResponse = false;
    };

    struct SharedPartyInstanceState
    {
        std::vector<PartyListEntryView> availableParties;
        std::vector<PartyMemberView> partyMembers;
        std::vector<ConnectedPlayerView> connectedPlayers;
        std::vector<JoinRequestView> joinRequests;
        std::vector<IncomingInviteView> incomingInvites;
        std::unordered_map<std::uint32_t, std::wstring> displayNamesByEntityId;
        std::wstring statusText;
        std::wstring loadingText;
        std::uint32_t partyId = 0;
        std::uint32_t leaderEntityId = 0;
        std::uint32_t selectedPartyId = 0;
        std::uint32_t selectedConnectedPlayerEntityId = 0;
        std::uint32_t selectedJoinRequestEntityId = 0;
        std::uint32_t selectedIncomingInvitePartyId = 0;
        std::uint32_t instanceId = 0;
        bool hasParty = false;
        bool loading = false;
        bool partyPopupVisible = false;
        PartyPopupTab activePopupTab = PartyPopupTab::InvitePlayers;
    };

    inline std::mutex g_partyInstanceStateMutex;
    inline SharedPartyInstanceState g_partyInstanceState{};
    inline PendingCommands g_pendingCommands{};

    inline std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();

        const int sourceLength = static_cast<int>(text.size());
        const int requiredLength = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), sourceLength, nullptr, 0);
        if (requiredLength <= 0)
            return std::wstring(text.begin(), text.end());

        std::wstring converted(static_cast<std::size_t>(requiredLength), L'\0');
        const int convertedLength = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            sourceLength,
            converted.data(),
            requiredLength);
        if (convertedLength <= 0)
            return std::wstring(text.begin(), text.end());

        return converted;
    }

    inline std::wstring PartyResultToStatusText(yuno::net::packets::PartyResultCode code)
    {
        using yuno::net::packets::PartyResultCode;
        switch (code)
        {
        case PartyResultCode::None:
            return std::wstring();
        case PartyResultCode::NotInWorld:
            return L"Enter the world first.";
        case PartyResultCode::AlreadyInParty:
            return L"Already in a party.";
        case PartyResultCode::PartyNotFound:
            return L"Party not found.";
        case PartyResultCode::PartyFull:
            return L"Party is full.";
        case PartyResultCode::NotPartyLeader:
            return L"Leader only action.";
        case PartyResultCode::AlreadyPending:
            return L"That action is already pending.";
        case PartyResultCode::InviteNotFound:
            return L"Invite not found.";
        case PartyResultCode::JoinRequestNotFound:
            return L"Join request not found.";
        case PartyResultCode::TargetNotFound:
            return L"Target player not found.";
        case PartyResultCode::CannotTargetSelf:
            return L"Cannot target yourself.";
        case PartyResultCode::TargetAlreadyInParty:
            return L"Target is already in a party.";
        case PartyResultCode::DuplicateInvite:
            return L"Invite already sent.";
        case PartyResultCode::DuplicateJoinRequest:
            return L"Join request already sent.";
        default:
            return L"Party action failed.";
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

    inline void SelectConnectedPlayer(std::uint32_t entityId)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.selectedConnectedPlayerEntityId = entityId;
    }

    inline void SelectJoinRequest(std::uint32_t entityId)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.selectedJoinRequestEntityId = entityId;
    }

    inline void SelectIncomingInvite(std::uint32_t partyId)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.selectedIncomingInvitePartyId = partyId;
    }

    inline void OpenPartyPopup()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.partyPopupVisible = true;
        g_pendingCommands.browsePartySocial = true;
        if (!g_partyInstanceState.hasParty)
            g_partyInstanceState.activePopupTab = PartyPopupTab::InvitePlayers;
        g_partyInstanceState.statusText = L"Party popup opened.";
    }

    inline void ClosePartyPopup()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.partyPopupVisible = false;
        g_partyInstanceState.statusText = L"Party popup closed.";
    }

    inline void SetPartyPopupTab(PartyPopupTab tab)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_partyInstanceState.partyPopupVisible = true;
        g_partyInstanceState.activePopupTab = tab;
        g_pendingCommands.browsePartySocial = true;
    }

    inline void QueueConfirmCreateParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        if (g_partyInstanceState.hasParty)
        {
            g_partyInstanceState.statusText = L"Already in a party.";
            return;
        }

        g_pendingCommands.createParty = true;
        g_partyInstanceState.statusText = L"Creating party...";
    }

    inline void QueueRefreshPartyList()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.refreshPartyList = true;
        g_partyInstanceState.statusText = L"Refreshing party list...";
    }

    inline void QueueRefreshPartySocial()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.browsePartySocial = true;
    }

    inline void QueueApplySelectedParty()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.applyPartyId = g_partyInstanceState.selectedPartyId;
        if (g_pendingCommands.applyPartyId != 0)
            g_partyInstanceState.statusText = L"Sending join request...";
        else
            g_partyInstanceState.statusText = L"Select a party from the list first.";
    }

    inline void QueueInviteSelectedPlayer()
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.inviteTargetEntityId = g_partyInstanceState.selectedConnectedPlayerEntityId;
        if (g_pendingCommands.inviteTargetEntityId != 0)
            g_partyInstanceState.statusText = L"Sending party invite...";
        else
            g_partyInstanceState.statusText = L"Select a connected player first.";
    }

    inline void QueueRespondToSelectedJoinRequest(bool accept)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.respondJoinRequestEntityId = g_partyInstanceState.selectedJoinRequestEntityId;
        if (g_pendingCommands.respondJoinRequestEntityId == 0)
        {
            g_partyInstanceState.statusText = L"Select a join request first.";
            g_pendingCommands.hasJoinRequestResponse = false;
            return;
        }

        g_pendingCommands.respondJoinRequestAccepted = accept;
        g_pendingCommands.hasJoinRequestResponse = true;
        g_partyInstanceState.statusText = accept ? L"Accepting join request..." : L"Rejecting join request...";
    }

    inline void QueueRespondToSelectedInvite(bool accept)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);
        g_pendingCommands.respondInvitePartyId = g_partyInstanceState.selectedIncomingInvitePartyId;
        if (g_pendingCommands.respondInvitePartyId == 0)
        {
            g_partyInstanceState.statusText = L"Select an invite first.";
            g_pendingCommands.hasInviteResponse = false;
            return;
        }

        g_pendingCommands.respondInviteAccepted = accept;
        g_pendingCommands.hasInviteResponse = true;
        g_partyInstanceState.statusText = accept ? L"Accepting party invite..." : L"Rejecting party invite...";
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

        const std::wstring status = PartyResultToStatusText(packet.resultCode);
        if (!status.empty())
        {
            g_partyInstanceState.statusText = status;
        }
        else
        {
            g_partyInstanceState.statusText = g_partyInstanceState.hasParty ? L"Party updated." : L"Party cleared.";
        }
    }

    inline void PublishPartySocialState(const yuno::net::packets::S2C_PartySocialState& packet)
    {
        std::lock_guard<std::mutex> lock(g_partyInstanceStateMutex);

        g_partyInstanceState.connectedPlayers.clear();
        g_partyInstanceState.connectedPlayers.reserve(packet.connectedPlayers.size());
        for (const auto& player : packet.connectedPlayers)
        {
            ConnectedPlayerView view{};
            view.entityId = player.entityId;
            view.displayName = Utf8ToWide(player.displayName);
            g_partyInstanceState.connectedPlayers.push_back(view);
        }

        g_partyInstanceState.joinRequests.clear();
        g_partyInstanceState.joinRequests.reserve(packet.joinRequests.size());
        for (const auto& request : packet.joinRequests)
        {
            JoinRequestView view{};
            view.applicantEntityId = request.applicantEntityId;
            view.displayName = Utf8ToWide(request.displayName);
            g_partyInstanceState.joinRequests.push_back(view);
        }

        g_partyInstanceState.incomingInvites.clear();
        g_partyInstanceState.incomingInvites.reserve(packet.incomingInvites.size());
        for (const auto& invite : packet.incomingInvites)
        {
            IncomingInviteView view{};
            view.partyId = invite.partyId;
            view.leaderEntityId = invite.leaderEntityId;
            view.leaderName = Utf8ToWide(invite.leaderName);
            g_partyInstanceState.incomingInvites.push_back(view);
        }

        auto connectedIt = std::find_if(
            g_partyInstanceState.connectedPlayers.begin(),
            g_partyInstanceState.connectedPlayers.end(),
            [](const ConnectedPlayerView& player)
            {
                return player.entityId == g_partyInstanceState.selectedConnectedPlayerEntityId;
            });
        if (connectedIt == g_partyInstanceState.connectedPlayers.end())
            g_partyInstanceState.selectedConnectedPlayerEntityId = 0;

        auto requestIt = std::find_if(
            g_partyInstanceState.joinRequests.begin(),
            g_partyInstanceState.joinRequests.end(),
            [](const JoinRequestView& request)
            {
                return request.applicantEntityId == g_partyInstanceState.selectedJoinRequestEntityId;
            });
        if (requestIt == g_partyInstanceState.joinRequests.end())
            g_partyInstanceState.selectedJoinRequestEntityId = 0;

        auto inviteIt = std::find_if(
            g_partyInstanceState.incomingInvites.begin(),
            g_partyInstanceState.incomingInvites.end(),
            [](const IncomingInviteView& invite)
            {
                return invite.partyId == g_partyInstanceState.selectedIncomingInvitePartyId;
            });
        if (inviteIt == g_partyInstanceState.incomingInvites.end())
            g_partyInstanceState.selectedIncomingInvitePartyId = 0;

        if (!packet.statusText.empty())
        {
            g_partyInstanceState.statusText = Utf8ToWide(packet.statusText);
        }
        else
        {
            const std::wstring status = PartyResultToStatusText(packet.resultCode);
            if (!status.empty())
                g_partyInstanceState.statusText = status;
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
        out.connectedPlayers = g_partyInstanceState.connectedPlayers;
        out.joinRequests = g_partyInstanceState.joinRequests;
        out.incomingInvites = g_partyInstanceState.incomingInvites;
        out.displayNamesByEntityId = g_partyInstanceState.displayNamesByEntityId;
        out.statusText = g_partyInstanceState.statusText;
        out.loadingText = g_partyInstanceState.loadingText;
        out.partyId = g_partyInstanceState.partyId;
        out.leaderEntityId = g_partyInstanceState.leaderEntityId;
        out.selectedPartyId = g_partyInstanceState.selectedPartyId;
        out.selectedConnectedPlayerEntityId = g_partyInstanceState.selectedConnectedPlayerEntityId;
        out.selectedJoinRequestEntityId = g_partyInstanceState.selectedJoinRequestEntityId;
        out.selectedIncomingInvitePartyId = g_partyInstanceState.selectedIncomingInvitePartyId;
        out.instanceId = g_partyInstanceState.instanceId;
        out.hasParty = g_partyInstanceState.hasParty;
        out.instanceActive = g_partyInstanceState.instanceId != 0;
        out.loading = g_partyInstanceState.loading;
        out.partyPopupVisible = g_partyInstanceState.partyPopupVisible;
        out.activePopupTab = g_partyInstanceState.activePopupTab;
        return true;
    }
}
