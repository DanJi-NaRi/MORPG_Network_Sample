#pragma once

#include <cstdint>
#include <array>
#include <unordered_map>
#include <vector>

#include "SceneBase.h"

class Player;
class Button;
class Text;

class TestScene final : public SceneBase
{
public:
    void OnEnter() override;
    void OnExit() override;

    void Update(float dt) override;
    void SubmitObj() override;
    void SubmitUI() override;

    const char* GetDebugName() const override { return "TestScene"; }

protected:
    bool OnCreateScene() override;
    void OnDestroyScene() override;

private:
    struct PlayerVisualRuntime
    {
        Player* visual = nullptr;
        XMFLOAT3 renderPos{ 0.0f, 0.0f, 0.0f };
        XMFLOAT3 targetPos{ 0.0f, 0.0f, 0.0f };
        bool initialized = false;
        bool isLocal = false;
    };

    PlayerVisualRuntime* EnsurePlayerVisual(std::uint32_t entityId, float x, float y, float z);
    bool ConsumeButtonPress(Button* button, bool& inOutPrevPressed);
    void CreateHud();
    void RefreshHud();

private:
    static constexpr float kRemoteInterpolationRate = 10.0f;
    static constexpr float kLocalCorrectionRate = 9.0f;
    static constexpr float kPlayerVisualYOffset = 1.0f;

    std::unordered_map<std::uint32_t, PlayerVisualRuntime> m_playerVisuals;
    Button* m_createPartyButton = nullptr;
    Button* m_refreshPartyListButton = nullptr;
    Button* m_joinSelectedButton = nullptr;
    Button* m_inviteSelectedButton = nullptr;
    Button* m_acceptInviteButton = nullptr;
    Button* m_leavePartyButton = nullptr;
    Button* m_enterInstanceButton = nullptr;
    std::array<Button*, 3> m_partyRowButtons{};
    std::array<Text*, 3> m_partyRowTexts{};
    std::array<bool, 3> m_prevPartyRowPressed{};
    Text* m_titleText = nullptr;
    Text* m_statusText = nullptr;
    Text* m_loadingText = nullptr;
    Text* m_partyText = nullptr;
    Text* m_pendingInviteText = nullptr;
    Text* m_rosterText = nullptr;
    Text* m_controlHintText = nullptr;
    bool m_prevCreatePartyPressed = false;
    bool m_prevRefreshPressed = false;
    bool m_prevJoinSelectedPressed = false;
    bool m_prevInviteSelectedPressed = false;
    bool m_prevAcceptInvitePressed = false;
    bool m_prevLeavePartyPressed = false;
    bool m_prevEnterInstancePressed = false;
};
