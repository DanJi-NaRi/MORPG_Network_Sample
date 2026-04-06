#pragma once

#include <cstdint>

#include "IGameApp.h"
#include "IAudioManager.h"
#include "YunoClientNetwork.h"

class GameApp : public IGameApp
{
public:
    GameApp() = default;
    ~GameApp() override;

    bool OnInit() override;
    void OnUpdate(float dt) override;
    void OnFixedUpdate(float fixedDt) override;
    void OnShutdown() override;


private:
    static constexpr float kInputSendIntervalSeconds = 1.0f / 30.0f;
    static constexpr float kClickMoveStopDistance = 0.35f;
    static constexpr float kClickMoveAxisDeadZone = 0.05f;

    yuno::game::YunoClientNetwork m_clientNet;
    bool m_enterWorldRequested = false;
    std::uint32_t m_moveClientTick = 0;
    std::uint32_t m_moveSequence = 0;
    float m_inputSendAccumulator = 0.0f;
    bool m_clickMoveActive = false;
    float m_clickMoveTargetX = 0.0f;
    float m_clickMoveTargetZ = 0.0f;
    //std::unique_ptr<GameManager> m_gameManager;

    bool TryPickGroundPointFromMouse(float& outX, float& outY, float& outZ) const;
    void ApplyClickMoveToInput(float& inOutMoveX, float& inOutMoveY);
};


void CameraMove(float dt);
