#include "pch.h"

#include "Player.h"

#include "ObjectManager.h"
#include "ObjectTypeRegistry.h"

namespace
{
    struct AutoReg_Player
    {
        AutoReg_Player()
        {
            ObjectTypeRegistry::Instance().Register(
                L"Player",
                [](ObjectManager& om, const UnitDesc& d)
                {
                    om.CreateObjectInternal<Player>(d);
                });
        }
    } s_reg_Player;
}

Player::Player()
{
    unitType = L"Player";
}

Player::~Player() = default;

bool Player::Update(float dTime)
{
    Unit::Update(dTime);
    return true;
}

bool Player::Submit(float dTime)
{
    Unit::Submit(dTime);
    return true;
}
