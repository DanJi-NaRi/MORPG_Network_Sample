#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace yuno::game
{
    struct WorldEntityState
    {
        std::uint32_t entityId = 0;
        std::uint32_t archetypeId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct SharedWorldState
    {
        bool hasLocalPlayer = false;
        std::uint32_t localEntityId = 0;
        std::int16_t localInputMoveX = 0;
        std::int16_t localInputMoveY = 0;
        std::vector<WorldEntityState> entities;
        bool snapshotDirty = false;
    };

    inline std::mutex g_worldStateMutex;
    inline SharedWorldState g_worldState{};

    inline void PublishLocalPlayerSpawn(std::uint32_t entityId, std::uint32_t archetypeId, float x, float y, float z)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        g_worldState.hasLocalPlayer = true;
        g_worldState.localEntityId = entityId;

        bool found = false;
        for (auto& entity : g_worldState.entities)
        {
            if (entity.entityId != entityId)
                continue;

            entity.archetypeId = archetypeId;
            entity.x = x;
            entity.y = y;
            entity.z = z;
            found = true;
            break;
        }

        if (!found)
        {
            WorldEntityState local{};
            local.entityId = entityId;
            local.archetypeId = archetypeId;
            local.x = x;
            local.y = y;
            local.z = z;
            g_worldState.entities.push_back(local);
        }

        g_worldState.snapshotDirty = true;
    }

    inline void PublishWorldSnapshot(const std::vector<WorldEntityState>& entities)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);

        std::vector<WorldEntityState> merged = entities;

        for (auto& entity : merged)
        {
            if (entity.entityId != g_worldState.localEntityId)
                continue;

            for (const auto& prev : g_worldState.entities)
            {
                if (prev.entityId != entity.entityId)
                    continue;

                entity.archetypeId = prev.archetypeId;
                break;
            }
        }

        g_worldState.entities = std::move(merged);
        g_worldState.snapshotDirty = true;
    }

    inline bool ConsumeWorldSnapshot(std::vector<WorldEntityState>& outEntities)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (!g_worldState.snapshotDirty)
            return false;

        outEntities = g_worldState.entities;
        g_worldState.snapshotDirty = false;
        return true;
    }

    inline bool TryGetLatestWorldSnapshot(std::vector<WorldEntityState>& outEntities)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (g_worldState.entities.empty())
            return false;

        outEntities = g_worldState.entities;
        return true;
    }

    inline bool TryGetLocalPlayerEntityId(std::uint32_t& outEntityId)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (!g_worldState.hasLocalPlayer || g_worldState.localEntityId == 0)
            return false;

        outEntityId = g_worldState.localEntityId;
        return true;
    }

    inline void PublishLocalInputAxis(std::int16_t moveX, std::int16_t moveY)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        g_worldState.localInputMoveX = moveX;
        g_worldState.localInputMoveY = moveY;
    }

    inline void GetLocalInputAxis(std::int16_t& outMoveX, std::int16_t& outMoveY)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        outMoveX = g_worldState.localInputMoveX;
        outMoveY = g_worldState.localInputMoveY;
    }
}
