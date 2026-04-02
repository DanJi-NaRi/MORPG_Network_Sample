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
    yuno::game::YunoClientNetwork m_clientNet;
    bool m_enterWorldRequested = false;
    std::uint32_t m_moveClientTick = 0;
    std::uint32_t m_moveSequence = 0;
    //std::unique_ptr<GameManager> m_gameManager;
};


void CameraMove(float dt);
