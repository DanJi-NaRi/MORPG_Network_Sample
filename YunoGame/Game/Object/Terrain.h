#pragma once

#include <cstdint>

#include "Unit.h"

class Terrain final : public Unit
{
public:
    Terrain();
    ~Terrain() override;

    bool BuildStaticPlaneMesh(std::uint32_t cols = 64, std::uint32_t rows = 64, float halfExtent = 60.0f);

    bool Update(float dTime = 0) override;
    bool Submit(float dTime = 0) override;
};

