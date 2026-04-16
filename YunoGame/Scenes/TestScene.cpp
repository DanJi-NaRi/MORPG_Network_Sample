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
#include "WorldPlayerState.h"
#include "YunoEngine.h"

namespace
{
    constexpr float kHudLeft = 24.0f;
    constexpr float kHudTop = 24.0f;
    constexpr float kHudWidth = 220.0f;
    constexpr float kButtonHeight = 34.0f;
}

bool TestScene::OnCreateScene()
{
    m_objectManager->CreateDirLight();

    Terrain* terrain = m_objectManager->CreateObject<Terrain>(
        L"Terrain_Main",
        XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (terrain && terrain->BuildStaticPlaneMesh(64, 64, 60.0f))
    {
        terrain->SetMaskColor(XMFLOAT4(0.1f, 0.3f, 0.10f, 1.0f));
    }

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
            {
                toRemove.push_back(kv.first);
            }
        }

        for (const std::uint32_t entityId : toRemove)
        {
            auto it = m_playerVisuals.find(entityId);
            if (it == m_playerVisuals.end())
                continue;

            Player* visual = it->second.visual;
            if (visual)
            {
                m_objectManager->DestroyObject(visual->GetID());
            }
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
            {
                runtime.targetPos = XMFLOAT3(predictedX, predictedY + kPlayerVisualYOffset, predictedZ);
            }

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

    yuno::game::HudSnapshot hud{};
    yuno::game::ConsumeHudSnapshot(hud);
    RefreshHud();
    SceneBase::Update(dt);

    if (ConsumeButtonPress(m_createPartyButton, m_prevCreatePartyPressed))
        yuno::game::QueueCreateParty();
    if (ConsumeButtonPress(m_refreshPartyListButton, m_prevRefreshPressed))
        yuno::game::QueueRefreshPartyList();
    if (ConsumeButtonPress(m_joinSelectedButton, m_prevJoinSelectedPressed))
        yuno::game::QueueJoinSelectedParty();
    if (ConsumeButtonPress(m_inviteSelectedButton, m_prevInviteSelectedPressed))
        yuno::game::QueueInviteSelectedParty();
    if (ConsumeButtonPress(m_acceptInviteButton, m_prevAcceptInvitePressed))
        yuno::game::QueueAcceptInvite();
    if (ConsumeButtonPress(m_leavePartyButton, m_prevLeavePartyPressed))
        yuno::game::QueueLeaveParty();
    if (ConsumeButtonPress(m_enterInstanceButton, m_prevEnterInstancePressed))
        yuno::game::QueueEnterInstance();

    yuno::game::ConsumeHudSnapshot(hud);
    for (std::size_t i = 0; i < m_partyRowButtons.size(); ++i)
    {
        if (ConsumeButtonPress(m_partyRowButtons[i], m_prevPartyRowPressed[i]) && i < hud.availableParties.size())
            yuno::game::SelectParty(hud.availableParties[i].partyId);
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
    {
        return &it->second;
    }

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
    m_titleText = CreateWidget<Text>(L"PartyHudTitle", Float2(320.0f, 26.0f), XMFLOAT3(kHudLeft, kHudTop, 0.0f));
    m_titleText->SetText(L"Party / Instance Demo UI");
    m_titleText->SetColor(XMFLOAT4(1.0f, 0.95f, 0.75f, 1.0f));
    m_titleText->SetLayer(WidgetLayer::HUD);

    m_statusText = CreateWidget<Text>(L"PartyHudStatus", Float2(360.0f, 48.0f), XMFLOAT3(kHudLeft, kHudTop + 28.0f, 0.0f));
    m_statusText->SetLayer(WidgetLayer::HUD);

    m_loadingText = CreateWidget<Text>(L"PartyHudLoading", Float2(420.0f, 28.0f), XMFLOAT3(700.0f, 24.0f, 0.0f));
    m_loadingText->SetColor(XMFLOAT4(1.0f, 0.85f, 0.35f, 1.0f));
    m_loadingText->SetLayer(WidgetLayer::Modal);

    m_partyText = CreateWidget<Text>(L"PartyHudParty", Float2(360.0f, 72.0f), XMFLOAT3(kHudLeft, kHudTop + 76.0f, 0.0f));
    m_partyText->SetLayer(WidgetLayer::HUD);

    m_pendingInviteText = CreateWidget<Text>(L"PartyHudInvite", Float2(360.0f, 28.0f), XMFLOAT3(kHudLeft, kHudTop + 150.0f, 0.0f));
    m_pendingInviteText->SetColor(XMFLOAT4(0.85f, 1.0f, 0.85f, 1.0f));
    m_pendingInviteText->SetLayer(WidgetLayer::HUD);

    m_rosterText = CreateWidget<Text>(L"PartyHudRoster", Float2(420.0f, 120.0f), XMFLOAT3(kHudLeft, kHudTop + 176.0f, 0.0f));
    m_rosterText->SetLayer(WidgetLayer::HUD);

    m_controlHintText = CreateWidget<Text>(L"PartyHudControls", Float2(520.0f, 44.0f), XMFLOAT3(kHudLeft, 690.0f, 0.0f));
    m_controlHintText->SetText(L"Movement: Arrow keys only. Right mouse button rotates camera.");
    m_controlHintText->SetLayer(WidgetLayer::HUD);

    m_createPartyButton = CreateWidget<Button>(L"CreatePartyButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 410.0f, 0.0f));
    m_refreshPartyListButton = CreateWidget<Button>(L"RefreshPartyListButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 450.0f, 0.0f));
    m_joinSelectedButton = CreateWidget<Button>(L"JoinSelectedPartyButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 490.0f, 0.0f));
    m_inviteSelectedButton = CreateWidget<Button>(L"InviteSelectedPartyButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 530.0f, 0.0f));
    m_acceptInviteButton = CreateWidget<Button>(L"AcceptInviteButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 570.0f, 0.0f));
    m_leavePartyButton = CreateWidget<Button>(L"LeavePartyButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 610.0f, 0.0f));
    m_enterInstanceButton = CreateWidget<Button>(L"EnterInstanceButton", Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, 650.0f, 0.0f));

    const std::array<std::wstring, 7> labels = {
        L"Create Party",
        L"Search Parties",
        L"Join Selected Party",
        L"Invite Selected Party",
        L"Accept Invite",
        L"Leave Party",
        L"Leader Enter Instance"
    };
    Button* buttons[7] = {
        m_createPartyButton,
        m_refreshPartyListButton,
        m_joinSelectedButton,
        m_inviteSelectedButton,
        m_acceptInviteButton,
        m_leavePartyButton,
        m_enterInstanceButton
    };

    for (int i = 0; i < 7; ++i)
    {
        buttons[i]->SetLayer(WidgetLayer::Panels);
        Text* label = CreateWidget<Text>(L"ActionLabel" + std::to_wstring(i), Float2(kHudWidth, 22.0f), XMFLOAT3(kHudLeft + 12.0f, 417.0f + (40.0f * i), 0.0f));
        label->SetLayer(WidgetLayer::Tooltip);
        label->SetText(labels[i]);
    }

    for (std::size_t i = 0; i < m_partyRowButtons.size(); ++i)
    {
        const float top = 270.0f + static_cast<float>(i) * 42.0f;
        m_partyRowButtons[i] = CreateWidget<Button>(L"PartyRowButton" + std::to_wstring(i), Float2(kHudWidth, kButtonHeight), XMFLOAT3(kHudLeft, top, 0.0f));
        m_partyRowButtons[i]->SetLayer(WidgetLayer::Panels);
        m_partyRowTexts[i] = CreateWidget<Text>(L"PartyRowText" + std::to_wstring(i), Float2(kHudWidth, 22.0f), XMFLOAT3(kHudLeft + 12.0f, top + 8.0f, 0.0f));
        m_partyRowTexts[i]->SetLayer(WidgetLayer::Tooltip);
    }

    RefreshHud();
}

void TestScene::RefreshHud()
{
    yuno::game::HudSnapshot hud{};
    yuno::game::ConsumeHudSnapshot(hud);

    if (m_statusText)
        m_statusText->SetText(hud.statusText.empty() ? L"Waiting for party actions..." : hud.statusText);

    if (m_loadingText)
    {
        m_loadingText->SetText(hud.loading ? hud.loadingText : L"");
        m_loadingText->SetVisible(hud.loading ? Visibility::Visible : Visibility::Hidden);
    }

    if (m_partyText)
    {
        std::wstring partySummary = hud.hasParty
            ? (L"Party " + std::to_wstring(hud.partyId) + L" | Leader Entity " + std::to_wstring(hud.leaderEntityId))
            : L"Not in a party.";
        if (hud.instanceActive)
            partySummary += L" | Instance " + std::to_wstring(hud.instanceId);
        m_partyText->SetText(partySummary);
    }

    if (m_pendingInviteText)
        m_pendingInviteText->SetText(hud.pendingInviteText);

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
        if (m_partyRowButtons[i])
            m_partyRowButtons[i]->SetVisible(hasEntry ? Visibility::Visible : Visibility::Hidden);
        if (m_partyRowTexts[i])
        {
            if (hasEntry)
            {
                const auto& entry = hud.availableParties[i];
                std::wstring prefix = (entry.partyId == hud.selectedPartyId) ? L"> " : L"  ";
                m_partyRowTexts[i]->SetText(prefix + L"Party " + std::to_wstring(entry.partyId) + L" | " + entry.leaderName + L" | members " + std::to_wstring(entry.memberCount));
                m_partyRowTexts[i]->SetVisible(Visibility::Visible);
            }
            else
            {
                m_partyRowTexts[i]->SetText(L"");
                m_partyRowTexts[i]->SetVisible(Visibility::Hidden);
            }
        }
    }

    std::uint32_t localEntityId = 0;
    const bool hasLocalEntity = yuno::game::TryGetLocalPlayerEntityId(localEntityId);
    const bool isLeader = hasLocalEntity && hud.hasParty && (hud.leaderEntityId == localEntityId);

    if (m_joinSelectedButton)
        m_joinSelectedButton->SetVisible(Visibility::Visible);
    if (m_inviteSelectedButton)
        m_inviteSelectedButton->SetVisible(Visibility::Visible);
    if (m_acceptInviteButton)
        m_acceptInviteButton->SetVisible(Visibility::Visible);
    if (m_leavePartyButton)
        m_leavePartyButton->SetVisible(Visibility::Visible);
    if (m_enterInstanceButton)
        m_enterInstanceButton->SetVisible((hud.hasParty && isLeader) ? Visibility::Visible : Visibility::Hidden);
}
