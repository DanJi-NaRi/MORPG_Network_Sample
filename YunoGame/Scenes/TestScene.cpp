#include "pch.h"

#include "TestScene.h"

#include <algorithm>
#include <cmath>

#include "Button.h"
#include "../Game/PartyInstanceClientState.h"
#include "Player.h"
#include "ObjectManager.h"
#include "Terrain.h"
#include "Text.h"
#include "UIManager.h"
#include "UIScope.h"
#include "WorldPlayerState.h"
#include "YunoEngine.h"

namespace
{
    constexpr float kLeftColumnX = 24.0f;
    constexpr float kTopY = 24.0f;
    constexpr float kPartyListWidth = 320.0f;
    constexpr float kActionColumnX = 930.0f;
    constexpr float kActionWidth = 250.0f;
    constexpr float kButtonHeight = 34.0f;
    constexpr float kRosterX = 1400.0f;
    constexpr float kPopupX = 360.0f;
    constexpr float kPopupY = 118.0f;
    constexpr float kPopupWidth = 1000.0f;
    constexpr float kPopupHeight = 700.0f;
    constexpr float kPartyUiOpacityPercent = 80.0f;
    constexpr float kPartyUiAlpha = kPartyUiOpacityPercent / 100.0f;
    constexpr XMFLOAT4 kHudButtonColor = XMFLOAT4(0.08f, 0.12f, 0.20f, kPartyUiAlpha);
    constexpr XMFLOAT4 kHudButtonLabelColor = XMFLOAT4(0.95f, 0.97f, 1.0f, 1.0f);
    constexpr XMFLOAT4 kPopupPanelColor = XMFLOAT4(0.04f, 0.06f, 0.10f, kPartyUiAlpha);
    constexpr XMFLOAT4 kPopupButtonColor = XMFLOAT4(0.12f, 0.18f, 0.30f, kPartyUiAlpha);
    constexpr XMFLOAT4 kPopupLabelColor = XMFLOAT4(0.96f, 0.97f, 1.0f, 1.0f);
    constexpr const wchar_t* kPartyUiTexturePath = L"../Assets/Textures/black.png";
}

bool TestScene::OnCreateScene()
{
    m_objectManager->CreateDirLight();

    Terrain* terrain = m_objectManager->CreateObject<Terrain>(
        L"Terrain_Main",
        XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (terrain && terrain->BuildStaticPlaneMesh(64, 64, 60.0f))
        terrain->SetMaskColor(XMFLOAT4(0.1f, 0.3f, 0.10f, 1.0f));

    CreateHud();
    return true;
}

void TestScene::OnDestroyScene()
{
    m_playerVisuals.clear();
}

void TestScene::OnEnter()
{
}

void TestScene::OnExit()
{
}

void TestScene::SetButtonVisible(const UIButtonRuntime& button, bool visible)
{
    const Visibility state = visible ? Visibility::Visible : Visibility::Hidden;
    if (button.button)
        button.button->SetVisible(state);
    if (button.label)
        button.label->SetVisible(state);
}

void TestScene::Update(float dt)
{
    std::vector<yuno::game::WorldEntityState> snapshotEntities;
    if (yuno::game::ConsumeWorldSnapshot(snapshotEntities))
    {
        std::uint32_t localEntityId = 0;
        const bool hasLocalEntity = yuno::game::TryGetLocalPlayerEntityId(localEntityId);

        std::unordered_map<std::uint32_t, bool> aliveMap;
        aliveMap.reserve(snapshotEntities.size());

        for (const auto& entity : snapshotEntities)
        {
            PlayerVisualRuntime* runtime = EnsurePlayerVisual(entity.entityId, entity.x, entity.y, entity.z);
            if (runtime && runtime->visual)
            {
                runtime->targetPos = XMFLOAT3(entity.x, entity.y + kPlayerVisualYOffset, entity.z);
                runtime->isLocal = hasLocalEntity && (entity.entityId == localEntityId);

                if (!runtime->initialized)
                {
                    runtime->renderPos = runtime->targetPos;
                    runtime->visual->SetPos(runtime->renderPos);
                    runtime->initialized = true;
                }
            }

            aliveMap[entity.entityId] = true;
        }

        std::vector<std::uint32_t> toRemove;
        for (const auto& kv : m_playerVisuals)
        {
            if (aliveMap.find(kv.first) == aliveMap.end())
                toRemove.push_back(kv.first);
        }

        for (const std::uint32_t entityId : toRemove)
        {
            auto it = m_playerVisuals.find(entityId);
            if (it == m_playerVisuals.end())
                continue;

            Player* visual = it->second.visual;
            if (visual)
                m_objectManager->DestroyObject(visual->GetID());
            m_playerVisuals.erase(it);
        }
    }

    for (auto& kv : m_playerVisuals)
    {
        PlayerVisualRuntime& runtime = kv.second;
        if (!runtime.visual || !runtime.initialized)
            continue;

        if (runtime.isLocal)
        {
            float predictedX = runtime.targetPos.x;
            float predictedY = runtime.targetPos.y - kPlayerVisualYOffset;
            float predictedZ = runtime.targetPos.z;
            if (yuno::game::TryGetReconciledLocalPosition(predictedX, predictedY, predictedZ))
                runtime.targetPos = XMFLOAT3(predictedX, predictedY + kPlayerVisualYOffset, predictedZ);

            const float alpha = std::min(1.0f, kLocalCorrectionRate * dt);
            runtime.renderPos.x += (runtime.targetPos.x - runtime.renderPos.x) * alpha;
            runtime.renderPos.y += (runtime.targetPos.y - runtime.renderPos.y) * alpha;
            runtime.renderPos.z += (runtime.targetPos.z - runtime.renderPos.z) * alpha;
        }
        else
        {
            float sampledX = 0.0f;
            float sampledY = 0.0f;
            float sampledZ = 0.0f;
            if (yuno::game::TrySampleRemoteInterpolatedPosition(
                kv.first,
                yuno::game::kRemoteInterpolationDelaySeconds,
                sampledX,
                sampledY,
                sampledZ))
            {
                runtime.renderPos = XMFLOAT3(sampledX, sampledY + kPlayerVisualYOffset, sampledZ);
                runtime.targetPos = runtime.renderPos;
            }
            else
            {
                const float alpha = std::min(1.0f, kRemoteInterpolationRate * dt);
                runtime.renderPos.x += (runtime.targetPos.x - runtime.renderPos.x) * alpha;
                runtime.renderPos.y += (runtime.targetPos.y - runtime.renderPos.y) * alpha;
                runtime.renderPos.z += (runtime.targetPos.z - runtime.renderPos.z) * alpha;
            }
        }

        runtime.visual->SetPos(runtime.renderPos);
    }

    RefreshHud();
    SceneBase::Update(dt);

    yuno::game::HudSnapshot hud{};
    yuno::game::ConsumeHudSnapshot(hud);

    if (ConsumeButtonPress(m_createPartyButton.button, m_prevCreatePartyPressed))
        yuno::game::OpenPartyPopup();
    if (ConsumeButtonPress(m_refreshPartyListButton.button, m_prevRefreshPressed))
        yuno::game::QueueRefreshPartyList();
    if (ConsumeButtonPress(m_applySelectedButton.button, m_prevApplySelectedPressed))
        yuno::game::QueueApplySelectedParty();
    if (ConsumeButtonPress(m_leavePartyButton.button, m_prevLeavePartyPressed))
        yuno::game::QueueLeaveParty();
    if (ConsumeButtonPress(m_enterInstanceButton.button, m_prevEnterInstancePressed))
        yuno::game::QueueEnterInstance();

    for (std::size_t i = 0; i < m_partyRowButtons.size(); ++i)
    {
        if (ConsumeButtonPress(m_partyRowButtons[i].button, m_prevPartyRowPressed[i]) && i < hud.availableParties.size())
            yuno::game::SelectParty(hud.availableParties[i].partyId);
    }

    if (hud.partyPopupVisible)
    {
        if (ConsumeButtonPress(m_popupCloseButton.button, m_prevPopupClosePressed))
            yuno::game::ClosePartyPopup();
        if (ConsumeButtonPress(m_popupInviteTabButton.button, m_prevPopupInviteTabPressed))
            yuno::game::SetPartyPopupTab(yuno::game::PartyPopupTab::InvitePlayers);
        if (ConsumeButtonPress(m_popupJoinRequestsTabButton.button, m_prevPopupJoinRequestsTabPressed))
            yuno::game::SetPartyPopupTab(yuno::game::PartyPopupTab::JoinRequests);
        if (ConsumeButtonPress(m_popupIncomingInvitesTabButton.button, m_prevPopupIncomingInvitesTabPressed))
            yuno::game::SetPartyPopupTab(yuno::game::PartyPopupTab::IncomingInvites);
        if (ConsumeButtonPress(m_popupCreatePartyButton.button, m_prevPopupCreatePartyPressed))
            yuno::game::QueueConfirmCreateParty();

        for (std::size_t i = 0; i < m_popupRowButtons.size(); ++i)
        {
            if (!ConsumeButtonPress(m_popupRowButtons[i].button, m_prevPopupRowPressed[i]))
                continue;

            switch (hud.activePopupTab)
            {
            case yuno::game::PartyPopupTab::InvitePlayers:
                if (i < hud.connectedPlayers.size())
                    yuno::game::SelectConnectedPlayer(hud.connectedPlayers[i].entityId);
                break;
            case yuno::game::PartyPopupTab::JoinRequests:
                if (i < hud.joinRequests.size())
                    yuno::game::SelectJoinRequest(hud.joinRequests[i].applicantEntityId);
                break;
            case yuno::game::PartyPopupTab::IncomingInvites:
                if (i < hud.incomingInvites.size())
                    yuno::game::SelectIncomingInvite(hud.incomingInvites[i].partyId);
                break;
            }
        }

        if (ConsumeButtonPress(m_popupPrimaryActionButton.button, m_prevPopupPrimaryPressed))
        {
            switch (hud.activePopupTab)
            {
            case yuno::game::PartyPopupTab::InvitePlayers:
                yuno::game::QueueInviteSelectedPlayer();
                break;
            case yuno::game::PartyPopupTab::JoinRequests:
                yuno::game::QueueRespondToSelectedJoinRequest(true);
                break;
            case yuno::game::PartyPopupTab::IncomingInvites:
                yuno::game::QueueRespondToSelectedInvite(true);
                break;
            }
        }

        if (ConsumeButtonPress(m_popupSecondaryActionButton.button, m_prevPopupSecondaryPressed))
        {
            switch (hud.activePopupTab)
            {
            case yuno::game::PartyPopupTab::JoinRequests:
                yuno::game::QueueRespondToSelectedJoinRequest(false);
                break;
            case yuno::game::PartyPopupTab::IncomingInvites:
                yuno::game::QueueRespondToSelectedInvite(false);
                break;
            default:
                break;
            }
        }
    }
}

void TestScene::SubmitObj()
{
    SceneBase::SubmitObj();
}

void TestScene::SubmitUI()
{
    SceneBase::SubmitUI();
}

TestScene::PlayerVisualRuntime* TestScene::EnsurePlayerVisual(std::uint32_t entityId, float x, float y, float z)
{
    if (entityId == 0)
        return nullptr;

    auto it = m_playerVisuals.find(entityId);
    if (it != m_playerVisuals.end())
        return &it->second;

    const std::wstring name = L"Player_" + std::to_wstring(entityId);
    const float liftedY = y + kPlayerVisualYOffset;
    Player* created = m_objectManager->CreateObjectFromFile<Player>(
        name,
        XMFLOAT3(x, liftedY, z),
        L"../Assets/fbx/weapon/Blaster/Blaster.fbx");

    if (created)
    {
        created->SetScale(XMFLOAT3(3.0f, 3.0f, 3.0f));
        PlayerVisualRuntime runtime{};
        runtime.visual = created;
        runtime.renderPos = XMFLOAT3(x, liftedY, z);
        runtime.targetPos = XMFLOAT3(x, liftedY, z);
        runtime.initialized = true;
        m_playerVisuals.emplace(entityId, runtime);
        return &m_playerVisuals.find(entityId)->second;
    }

    return nullptr;
}

bool TestScene::ConsumeButtonPress(Button* button, bool& inOutPrevPressed)
{
    if (!button || button->IsHidden() || button->IsCollapsed())
    {
        inOutPrevPressed = false;
        return false;
    }

    const bool pressed = button->GetButtonState() == ButtonState::Pressed;
    const bool triggered = pressed && !inOutPrevPressed;
    inOutPrevPressed = pressed;
    return triggered;
}

void TestScene::CreateHud()
{
    UIScope ui(*this);

    UITextStyle titleStyle{};
    titleStyle.layer = WidgetLayer::HUD;
    titleStyle.color = XMFLOAT4(1.0f, 0.95f, 0.75f, 1.0f);
    m_titleText = ui.CreateText(L"PartyHudTitle", Float2(360.0f, 26.0f), XMFLOAT3(kLeftColumnX, kTopY, 0.0f), L"Party / Instance Demo UI", titleStyle);

    UITextStyle hudTextStyle{};
    hudTextStyle.layer = WidgetLayer::HUD;
    m_statusText = ui.CreateText(L"PartyHudStatus", Float2(520.0f, 54.0f), XMFLOAT3(kLeftColumnX, kTopY + 30.0f, 0.0f), L"", hudTextStyle);
    m_partyListTitleText = ui.CreateText(L"PartyListTitle", Float2(320.0f, 24.0f), XMFLOAT3(kLeftColumnX, 128.0f, 0.0f), L"Open Parties", hudTextStyle);
    m_partyText = ui.CreateText(L"PartyHudParty", Float2(300.0f, 66.0f), XMFLOAT3(kRosterX, kTopY + 8.0f, 0.0f), L"", hudTextStyle);
    m_rosterText = ui.CreateText(L"PartyHudRoster", Float2(320.0f, 240.0f), XMFLOAT3(kRosterX, kTopY + 88.0f, 0.0f), L"", hudTextStyle);

    UITextStyle loadingStyle{};
    loadingStyle.layer = WidgetLayer::Modal;
    loadingStyle.color = XMFLOAT4(1.0f, 0.85f, 0.35f, 1.0f);
    m_loadingText = ui.CreateText(L"PartyHudLoading", Float2(320.0f, 28.0f), XMFLOAT3(820.0f, 24.0f, 0.0f), L"", loadingStyle);

    m_controlHintText = ui.CreateText(
        L"PartyHudControls",
        Float2(620.0f, 48.0f),
        XMFLOAT3(kLeftColumnX, 1000.0f, 0.0f),
        L"Movement: Arrow keys only. Right mouse button rotates camera.",
        hudTextStyle);

    UIButtonStyle actionButtonStyle{};
    actionButtonStyle.buttonLayer = WidgetLayer::Panels;
    actionButtonStyle.buttonColor = kHudButtonColor;
    actionButtonStyle.labelLayer = WidgetLayer::Tooltip;
    actionButtonStyle.labelAnchor = UIDirection::Left;
    actionButtonStyle.labelPivot = UIDirection::Left;
    actionButtonStyle.labelColor = kHudButtonLabelColor;
    actionButtonStyle.labelOffset = XMFLOAT2(12.0f, 0.0f);
    actionButtonStyle.texturePath = kPartyUiTexturePath;

    UIButtonStyle sectionPanelStyle = actionButtonStyle;
    sectionPanelStyle.buttonColor = kPopupPanelColor;

    ui.CreateButton(
        L"PartyListPanel",
        Float2(kPartyListWidth + 24.0f, 260.0f),
        XMFLOAT3(kLeftColumnX - 12.0f, 120.0f, 0.0f),
        sectionPanelStyle);

    ui.CreateButton(
        L"PartyActionRailPanel",
        Float2((kActionWidth * 2.0f) + 24.0f, 250.0f),
        XMFLOAT3((kActionColumnX * 1.5f) - 12.0f, 306.0f, 0.0f),
        sectionPanelStyle);

    UIColumnLayout partyRows = ui.Column(kLeftColumnX, 164.0f, 8.0f);
    for (std::size_t i = 0; i < m_partyRowButtons.size(); ++i)
    {
        const UILabeledButton row = ui.CreateLabeledButton(
            L"PartyRowButton" + std::to_wstring(i),
            Float2(kPartyListWidth, kButtonHeight),
            partyRows.Push(Float2(kPartyListWidth, kButtonHeight)),
            L"",
            actionButtonStyle);
        m_partyRowButtons[i] = { row.button, row.label };
    }

    UIColumnLayout actionButtons = ui.Column(kActionColumnX*1.5f, 320.0f, 10.0f);
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyMenuButton",
            Float2(kActionWidth*2, kButtonHeight),
            actionButtons.Push(Float2(kActionWidth, kButtonHeight)),
            L"Party Menu",
            actionButtonStyle);
        m_createPartyButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"RefreshPartyListButton",
            Float2(kActionWidth, kButtonHeight),
            actionButtons.Push(Float2(kActionWidth, kButtonHeight)),
            L"Search Parties",
            actionButtonStyle);
        m_refreshPartyListButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"ApplyPartyButton",
            Float2(kActionWidth, kButtonHeight),
            actionButtons.Push(Float2(kActionWidth, kButtonHeight)),
            L"Apply To Selected Party",
            actionButtonStyle);
        m_applySelectedButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"LeavePartyButton",
            Float2(kActionWidth, kButtonHeight),
            actionButtons.Push(Float2(kActionWidth, kButtonHeight)),
            L"Leave Party",
            actionButtonStyle);
        m_leavePartyButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"EnterInstanceButton",
            Float2(kActionWidth, kButtonHeight),
            actionButtons.Push(Float2(kActionWidth, kButtonHeight)),
            L"Leader Enter Instance",
            actionButtonStyle);
        m_enterInstanceButton = { parts.button, parts.label };
    }

    UIButtonStyle popupButtonStyle = actionButtonStyle;
    popupButtonStyle.buttonLayer = WidgetLayer::Popups;
    popupButtonStyle.buttonColor = kPopupButtonColor;
    popupButtonStyle.labelLayer = WidgetLayer::Tooltip;
    popupButtonStyle.labelColor = kPopupLabelColor;
    popupButtonStyle.texturePath = kPartyUiTexturePath;

    m_partyPopupPanel = ui.CreateButton(
        L"PartyPopupPanel",
        Float2(kPopupWidth, kPopupHeight),
        XMFLOAT3(kPopupX, kPopupY, 0.0f),
        popupButtonStyle);
    if (m_partyPopupPanel)
    {
        m_partyPopupPanel->SetLayer(WidgetLayer::Popups);
        m_partyPopupPanel->SetColor(kPopupPanelColor);
    }

    UITextStyle popupTextStyle{};
    popupTextStyle.layer = WidgetLayer::Tooltip;
    m_popupTitleText = ui.CreateText(L"PartyPopupTitle", Float2(260.0f, 24.0f), XMFLOAT3(kPopupX + 20.0f, kPopupY + 16.0f, 0.0f), L"Party Management", popupTextStyle);
    m_popupHintText = ui.CreateText(L"PartyPopupHint", Float2(500.0f, 48.0f), XMFLOAT3(kPopupX + 20.0f, kPopupY + 74.0f, 0.0f), L"", popupTextStyle);
    m_popupEmptyText = ui.CreateText(L"PartyPopupEmpty", Float2(500.0f, 28.0f), XMFLOAT3(kPopupX + 20.0f, kPopupY + 340.0f, 0.0f), L"", popupTextStyle);

    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupCloseButton",
            Float2(42.0f, 28.0f),
            XMFLOAT3(kPopupX + kPopupWidth - 58.0f, kPopupY + 14.0f, 0.0f),
            L"X",
            popupButtonStyle);
        m_popupCloseButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupInviteTab",
            Float2(300.0f, kButtonHeight),
            XMFLOAT3(kPopupX + 20.0f, kPopupY + 116.0f, 0.0f),
            L"Invite Players",
            popupButtonStyle);
        m_popupInviteTabButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupJoinRequestsTab",
            Float2(300.0f, kButtonHeight),
            XMFLOAT3(kPopupX + 320.0f, kPopupY + 116.0f, 0.0f),
            L"Approve Requests",
            popupButtonStyle);
        m_popupJoinRequestsTabButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupIncomingInvitesTab",
            Float2(300.0f, kButtonHeight),
            XMFLOAT3(kPopupX + 720.0f, kPopupY + 116.0f, 0.0f),
            L"Received Invites",
            popupButtonStyle);
        m_popupIncomingInvitesTabButton = { parts.button, parts.label };
    }

    UIColumnLayout popupRows = ui.Column(kPopupX + 20.0f, kPopupY + 170.0f, 8.0f);
    for (std::size_t i = 0; i < m_popupRowButtons.size(); ++i)
    {
        const UILabeledButton row = ui.CreateLabeledButton(
            L"PartyPopupRowButton" + std::to_wstring(i),
            Float2(kPopupWidth - 40.0f, kButtonHeight),
            popupRows.Push(Float2(kPopupWidth - 40.0f, kButtonHeight)),
            L"",
            popupButtonStyle);
        m_popupRowButtons[i] = { row.button, row.label };
    }

    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupCreateButton",
            Float2(150.0f, kButtonHeight),
            XMFLOAT3(kPopupX + 20.0f, kPopupY + kPopupHeight - 54.0f, 0.0f),
            L"Create Party",
            popupButtonStyle);
        m_popupCreatePartyButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupPrimaryButton",
            Float2(150.0f, kButtonHeight),
            XMFLOAT3(kPopupX + kPopupWidth - 320.0f, kPopupY + kPopupHeight - 54.0f, 0.0f),
            L"Confirm",
            popupButtonStyle);
        m_popupPrimaryActionButton = { parts.button, parts.label };
    }
    {
        const UILabeledButton parts = ui.CreateLabeledButton(
            L"PartyPopupSecondaryButton",
            Float2(130.0f, kButtonHeight),
            XMFLOAT3(kPopupX + kPopupWidth - 150.0f, kPopupY + kPopupHeight - 54.0f, 0.0f),
            L"Reject",
            popupButtonStyle);
        m_popupSecondaryActionButton = { parts.button, parts.label };
    }

    RefreshHud();
}

void TestScene::RefreshHud()
{
    yuno::game::HudSnapshot hud{};
    yuno::game::ConsumeHudSnapshot(hud);

    if (m_statusText)
        m_statusText->SetText(hud.statusText.empty() ? L"Use Party Menu to open the social popup." : hud.statusText);

    if (m_loadingText)
    {
        m_loadingText->SetText(hud.loading ? hud.loadingText : L"");
        m_loadingText->SetVisible(hud.loading ? Visibility::Visible : Visibility::Hidden);
    }

    if (m_partyText)
    {
        std::wstring summary = hud.hasParty
            ? (L"Party " + std::to_wstring(hud.partyId) + L" | Leader Entity " + std::to_wstring(hud.leaderEntityId))
            : L"No active party.";
        if (hud.instanceActive)
            summary += L"\nInstance " + std::to_wstring(hud.instanceId) + L" is active.";
        m_partyText->SetText(summary);
    }

    if (m_rosterText)
    {
        std::wstring roster = L"Party Members:\n";
        if (hud.partyMembers.empty())
        {
            roster += L"- none";
        }
        else
        {
            for (const auto& member : hud.partyMembers)
            {
                roster += L"- ";
                roster += member.displayName.empty() ? (L"Entity " + std::to_wstring(member.entityId)) : member.displayName;
                roster += member.online ? L" [online]" : L" [offline]";
                roster += member.alive ? L" [alive]" : L" [down]";
                roster += L"\n";
            }
        }
        m_rosterText->SetText(roster);
    }

    for (std::size_t i = 0; i < m_partyRowButtons.size(); ++i)
    {
        const bool hasEntry = i < hud.availableParties.size();
        SetButtonVisible(m_partyRowButtons[i], hasEntry);
        if (!hasEntry || !m_partyRowButtons[i].label)
            continue;

        const auto& entry = hud.availableParties[i];
        const std::wstring prefix = (entry.partyId == hud.selectedPartyId) ? L"> " : L"  ";
        m_partyRowButtons[i].label->SetText(prefix + L"Party " + std::to_wstring(entry.partyId) + L" | " + entry.leaderName + L" | members " + std::to_wstring(entry.memberCount));
    }

    std::uint32_t localEntityId = 0;
    const bool hasLocalEntity = yuno::game::TryGetLocalPlayerEntityId(localEntityId);
    const bool isLeader = hasLocalEntity && hud.hasParty && (hud.leaderEntityId == localEntityId);

    SetButtonVisible(m_createPartyButton, true);
    SetButtonVisible(m_refreshPartyListButton, true);
    SetButtonVisible(m_applySelectedButton, !hud.hasParty);
    SetButtonVisible(m_leavePartyButton, hud.hasParty);
    SetButtonVisible(m_enterInstanceButton, hud.hasParty && isLeader);

    const Visibility popupVisibility = hud.partyPopupVisible ? Visibility::Visible : Visibility::Hidden;
    if (m_partyPopupPanel)
        m_partyPopupPanel->SetVisible(popupVisibility);
    if (m_popupTitleText)
        m_popupTitleText->SetVisible(popupVisibility);
    if (m_popupHintText)
        m_popupHintText->SetVisible(popupVisibility);
    if (m_popupEmptyText)
        m_popupEmptyText->SetVisible(popupVisibility);

    SetButtonVisible(m_popupCloseButton, hud.partyPopupVisible);
    SetButtonVisible(m_popupInviteTabButton, hud.partyPopupVisible);
    SetButtonVisible(m_popupJoinRequestsTabButton, hud.partyPopupVisible);
    SetButtonVisible(m_popupIncomingInvitesTabButton, hud.partyPopupVisible);

    if (m_popupInviteTabButton.label)
        m_popupInviteTabButton.label->SetText(hud.activePopupTab == yuno::game::PartyPopupTab::InvitePlayers ? L"> Invite Players <" : L"Invite Players");
    if (m_popupJoinRequestsTabButton.label)
        m_popupJoinRequestsTabButton.label->SetText(hud.activePopupTab == yuno::game::PartyPopupTab::JoinRequests ? L"> Approve Requests <" : L"Approve Requests");
    if (m_popupIncomingInvitesTabButton.label)
        m_popupIncomingInvitesTabButton.label->SetText(hud.activePopupTab == yuno::game::PartyPopupTab::IncomingInvites ? L"> Received Invites <" : L"Received Invites");

    std::wstring hintText;
    std::wstring emptyText;
    std::wstring primaryText;
    std::wstring secondaryText;
    bool showCreateAction = hud.partyPopupVisible && !hud.hasParty;
    bool showPrimaryAction = false;
    bool showSecondaryAction = false;

    for (auto& row : m_popupRowButtons)
        SetButtonVisible(row, false);

    switch (hud.activePopupTab)
    {
    case yuno::game::PartyPopupTab::InvitePlayers:
        hintText = !hud.hasParty ? L"Create a party first, then invite connected players." : (isLeader ? L"Select a connected player and send a party invite." : L"Only the party leader can send invites.");
        emptyText = hud.connectedPlayers.empty() ? L"No inviteable connected players." : L"";
        primaryText = L"Send Invite";
        showPrimaryAction = hud.partyPopupVisible && hud.hasParty && isLeader;
        for (std::size_t i = 0; i < m_popupRowButtons.size() && i < hud.connectedPlayers.size(); ++i)
        {
            SetButtonVisible(m_popupRowButtons[i], hud.partyPopupVisible);
            const std::wstring prefix = (hud.connectedPlayers[i].entityId == hud.selectedConnectedPlayerEntityId) ? L"> " : L"  ";
            m_popupRowButtons[i].label->SetText(prefix + hud.connectedPlayers[i].displayName);
        }
        break;
    case yuno::game::PartyPopupTab::JoinRequests:
        hintText = !hud.hasParty ? L"Create a party to receive join requests." : (isLeader ? L"Review join requests sent to your party." : L"Only the party leader can review join requests.");
        emptyText = hud.joinRequests.empty() ? L"No pending join requests." : L"";
        primaryText = L"Accept";
        secondaryText = L"Reject";
        showPrimaryAction = hud.partyPopupVisible && hud.hasParty && isLeader;
        showSecondaryAction = hud.partyPopupVisible && hud.hasParty && isLeader;
        for (std::size_t i = 0; i < m_popupRowButtons.size() && i < hud.joinRequests.size(); ++i)
        {
            SetButtonVisible(m_popupRowButtons[i], hud.partyPopupVisible);
            const std::wstring prefix = (hud.joinRequests[i].applicantEntityId == hud.selectedJoinRequestEntityId) ? L"> " : L"  ";
            m_popupRowButtons[i].label->SetText(prefix + hud.joinRequests[i].displayName + L" wants to join");
        }
        break;
    case yuno::game::PartyPopupTab::IncomingInvites:
        hintText = L"Review party invites sent to you.";
        emptyText = hud.incomingInvites.empty() ? L"No incoming invites." : L"";
        primaryText = L"Accept";
        secondaryText = L"Reject";
        showPrimaryAction = hud.partyPopupVisible;
        showSecondaryAction = hud.partyPopupVisible;
        for (std::size_t i = 0; i < m_popupRowButtons.size() && i < hud.incomingInvites.size(); ++i)
        {
            SetButtonVisible(m_popupRowButtons[i], hud.partyPopupVisible);
            const std::wstring prefix = (hud.incomingInvites[i].partyId == hud.selectedIncomingInvitePartyId) ? L"> " : L"  ";
            m_popupRowButtons[i].label->SetText(prefix + L"Party " + std::to_wstring(hud.incomingInvites[i].partyId) + L" | Leader " + hud.incomingInvites[i].leaderName);
        }
        break;
    }

    if (m_popupHintText)
        m_popupHintText->SetText(hintText);
    if (m_popupEmptyText)
        m_popupEmptyText->SetText(emptyText);
    if (m_popupPrimaryActionButton.label)
        m_popupPrimaryActionButton.label->SetText(primaryText);
    if (m_popupSecondaryActionButton.label)
        m_popupSecondaryActionButton.label->SetText(secondaryText);

    SetButtonVisible(m_popupCreatePartyButton, showCreateAction);
    SetButtonVisible(m_popupPrimaryActionButton, showPrimaryAction);
    SetButtonVisible(m_popupSecondaryActionButton, showSecondaryAction);
}
