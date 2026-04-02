#pragma once

#include "Unit.h"

class Player : public Unit
{
public:
    Player();
    ~Player() override;

    bool Update(float dTime = 0) override;
    bool Submit(float dTime = 0) override;
};
