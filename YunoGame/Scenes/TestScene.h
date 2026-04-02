#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "SceneBase.h"

class Player;

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

private:
    static constexpr float kRemoteInterpolationRate = 10.0f;
    static constexpr float kLocalCorrectionRate = 9.0f;
    static constexpr float kPlayerVisualYOffset = 1.0f;

    std::unordered_map<std::uint32_t, PlayerVisualRuntime> m_playerVisuals;
};
