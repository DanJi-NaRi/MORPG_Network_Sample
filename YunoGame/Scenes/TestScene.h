#pragma once

#include <array>
#include <cstdint>
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

    struct UIButtonRuntime
    {
        Button* button = nullptr;
        Text* label = nullptr;
    };

    PlayerVisualRuntime* EnsurePlayerVisual(std::uint32_t entityId, float x, float y, float z);
    bool ConsumeButtonPress(Button* button, bool& inOutPrevPressed);
    void SetButtonVisible(const UIButtonRuntime& button, bool visible);
    void CreateHud();
    void RefreshHud();

private:
    static constexpr float kRemoteInterpolationRate = 10.0f;
    static constexpr float kLocalCorrectionRate = 9.0f;
    static constexpr float kPlayerVisualYOffset = 1.0f;
    static constexpr std::size_t kVisiblePartyRows = 5;
    static constexpr std::size_t kVisiblePopupRows = 5;

    std::unordered_map<std::uint32_t, PlayerVisualRuntime> m_playerVisuals;

    UIButtonRuntime m_createPartyButton{};
    UIButtonRuntime m_refreshPartyListButton{};
    UIButtonRuntime m_applySelectedButton{};
    UIButtonRuntime m_leavePartyButton{};
    UIButtonRuntime m_enterInstanceButton{};
    std::array<UIButtonRuntime, kVisiblePartyRows> m_partyRowButtons{};

    Button* m_partyPopupPanel = nullptr;
    UIButtonRuntime m_popupCloseButton{};
    UIButtonRuntime m_popupInviteTabButton{};
    UIButtonRuntime m_popupJoinRequestsTabButton{};
    UIButtonRuntime m_popupIncomingInvitesTabButton{};
    UIButtonRuntime m_popupCreatePartyButton{};
    UIButtonRuntime m_popupPrimaryActionButton{};
    UIButtonRuntime m_popupSecondaryActionButton{};
    std::array<UIButtonRuntime, kVisiblePopupRows> m_popupRowButtons{};

    std::array<bool, kVisiblePartyRows> m_prevPartyRowPressed{};
    std::array<bool, kVisiblePopupRows> m_prevPopupRowPressed{};

    Text* m_titleText = nullptr;
    Text* m_statusText = nullptr;
    Text* m_loadingText = nullptr;
    Text* m_partyListTitleText = nullptr;
    Text* m_partyText = nullptr;
    Text* m_rosterText = nullptr;
    Text* m_controlHintText = nullptr;
    Text* m_popupTitleText = nullptr;
    Text* m_popupHintText = nullptr;
    Text* m_popupEmptyText = nullptr;

    bool m_prevCreatePartyPressed = false;
    bool m_prevRefreshPressed = false;
    bool m_prevApplySelectedPressed = false;
    bool m_prevLeavePartyPressed = false;
    bool m_prevEnterInstancePressed = false;
    bool m_prevPopupClosePressed = false;
    bool m_prevPopupInviteTabPressed = false;
    bool m_prevPopupJoinRequestsTabPressed = false;
    bool m_prevPopupIncomingInvitesTabPressed = false;
    bool m_prevPopupCreatePartyPressed = false;
    bool m_prevPopupPrimaryPressed = false;
    bool m_prevPopupSecondaryPressed = false;
};
