#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace yuno::game
{
    struct WorldEntityState
    {
        std::uint32_t entityId = 0;
        std::uint32_t archetypeId = 0;
        std::uint32_t lastProcessedInputSequence = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct PendingLocalInput
    {
        std::uint32_t sequence = 0;
        std::int16_t moveX = 0;
        std::int16_t moveY = 0;
    };

    struct SharedWorldState
    {
        bool hasLocalPlayer = false;
        std::uint32_t localEntityId = 0;

        float localServerX = 0.0f;
        float localServerY = 0.0f;
        float localServerZ = 0.0f;
        std::uint32_t localLastAckSequence = 0;

        std::vector<PendingLocalInput> pendingLocalInputs;
        std::vector<WorldEntityState> entities;
        struct TimedPositionSample
        {
            double timeSeconds = 0.0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };
        std::unordered_map<std::uint32_t, std::deque<TimedPositionSample>> remoteInterpolationBuffers;
        bool snapshotDirty = false;
    };

    inline constexpr float kPredictionSpeedUnitsPerSec = 2.25f;
    inline constexpr float kInputStepSeconds = 1.0f / 30.0f;
    inline constexpr float kRemoteInterpolationDelaySeconds = 0.10f;
    inline constexpr double kRemoteInterpolationMaxBufferSeconds = 1.0;

    inline std::mutex g_worldStateMutex;
    inline SharedWorldState g_worldState{};

    inline double GetSteadyTimeSeconds()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }

    inline void PublishLocalPlayerSpawn(std::uint32_t entityId, std::uint32_t archetypeId, float x, float y, float z)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        g_worldState.hasLocalPlayer = true;
        g_worldState.localEntityId = entityId;
        g_worldState.localServerX = x;
        g_worldState.localServerY = y;
        g_worldState.localServerZ = z;

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

    inline void PublishLocalInput(std::uint32_t sequence, std::int16_t moveX, std::int16_t moveY)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        PendingLocalInput input{};
        input.sequence = sequence;
        input.moveX = moveX;
        input.moveY = moveY;
        g_worldState.pendingLocalInputs.push_back(input);
    }

    inline void PublishWorldSnapshot(const std::vector<WorldEntityState>& entities)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        const double nowSeconds = GetSteadyTimeSeconds();

        std::vector<WorldEntityState> merged = entities;
        std::unordered_set<std::uint32_t> aliveEntities;
        aliveEntities.reserve(merged.size());

        for (auto& entity : merged)
        {
            aliveEntities.insert(entity.entityId);

            if (g_worldState.hasLocalPlayer && entity.entityId == g_worldState.localEntityId)
            {
                for (const auto& prev : g_worldState.entities)
                {
                    if (prev.entityId != entity.entityId)
                        continue;
                    entity.archetypeId = prev.archetypeId;
                    break;
                }

                g_worldState.localServerX = entity.x;
                g_worldState.localServerY = entity.y;
                g_worldState.localServerZ = entity.z;
                g_worldState.localLastAckSequence = entity.lastProcessedInputSequence;

                auto& pending = g_worldState.pendingLocalInputs;
                pending.erase(
                    std::remove_if(
                        pending.begin(),
                        pending.end(),
                        [ack = g_worldState.localLastAckSequence](const PendingLocalInput& input)
                        {
                            return input.sequence <= ack;
                        }),
                    pending.end());
                continue;
            }

            auto& samples = g_worldState.remoteInterpolationBuffers[entity.entityId];
            SharedWorldState::TimedPositionSample sample{};
            sample.timeSeconds = nowSeconds;
            sample.x = entity.x;
            sample.y = entity.y;
            sample.z = entity.z;
            samples.push_back(sample);

            while (samples.size() >= 2 && (nowSeconds - samples.front().timeSeconds) > kRemoteInterpolationMaxBufferSeconds)
            {
                samples.pop_front();
            }
        }

        for (auto it = g_worldState.remoteInterpolationBuffers.begin(); it != g_worldState.remoteInterpolationBuffers.end();)
        {
            if (aliveEntities.find(it->first) == aliveEntities.end() ||
                (g_worldState.hasLocalPlayer && it->first == g_worldState.localEntityId))
            {
                it = g_worldState.remoteInterpolationBuffers.erase(it);
                continue;
            }
            ++it;
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

    inline bool TryGetLocalPlayerEntityId(std::uint32_t& outEntityId)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (!g_worldState.hasLocalPlayer || g_worldState.localEntityId == 0)
            return false;

        outEntityId = g_worldState.localEntityId;
        return true;
    }

    inline bool TryGetReconciledLocalPosition(float& outX, float& outY, float& outZ)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (!g_worldState.hasLocalPlayer)
            return false;

        float x = g_worldState.localServerX;
        float y = g_worldState.localServerY;
        float z = g_worldState.localServerZ;

        for (const auto& input : g_worldState.pendingLocalInputs)
        {
            float inputX = 0.0f;
            float inputZ = 0.0f;

            if (input.moveX > 0)
                inputX = 1.0f;
            else if (input.moveX < 0)
                inputX = -1.0f;

            if (input.moveY > 0)
                inputZ = 1.0f;
            else if (input.moveY < 0)
                inputZ = -1.0f;

            const float lenSq = inputX * inputX + inputZ * inputZ;
            if (lenSq <= 0.0f)
                continue;

            const float invLen = 1.0f / std::sqrt(lenSq);
            x += (inputX * invLen) * kPredictionSpeedUnitsPerSec * kInputStepSeconds;
            z += (inputZ * invLen) * kPredictionSpeedUnitsPerSec * kInputStepSeconds;
        }

        outX = x;
        outY = y;
        outZ = z;
        return true;
    }

    inline bool TrySampleRemoteInterpolatedPosition(std::uint32_t entityId, float delaySeconds, float& outX, float& outY, float& outZ)
    {
        std::lock_guard<std::mutex> lock(g_worldStateMutex);
        if (entityId == 0)
            return false;
        if (g_worldState.hasLocalPlayer && entityId == g_worldState.localEntityId)
            return false;

        auto it = g_worldState.remoteInterpolationBuffers.find(entityId);
        if (it == g_worldState.remoteInterpolationBuffers.end() || it->second.empty())
            return false;

        auto& samples = it->second;
        const double nowSeconds = GetSteadyTimeSeconds();
        const double safeDelay = std::max(0.0, static_cast<double>(delaySeconds));
        const double renderTime = nowSeconds - safeDelay;

        while (samples.size() >= 2 && samples[1].timeSeconds <= renderTime)
        {
            samples.pop_front();
        }

        if (samples.size() == 1)
        {
            outX = samples.front().x;
            outY = samples.front().y;
            outZ = samples.front().z;
            return true;
        }

        const auto& a = samples[0];
        const auto& b = samples[1];
        if (renderTime <= a.timeSeconds || b.timeSeconds <= a.timeSeconds)
        {
            outX = a.x;
            outY = a.y;
            outZ = a.z;
            return true;
        }

        const double span = b.timeSeconds - a.timeSeconds;
        double alpha = (renderTime - a.timeSeconds) / span;
        alpha = std::clamp(alpha, 0.0, 1.0);
        const float t = static_cast<float>(alpha);

        outX = a.x + (b.x - a.x) * t;
        outY = a.y + (b.y - a.y) * t;
        outZ = a.z + (b.z - a.z) * t;
        return true;
    }
}
