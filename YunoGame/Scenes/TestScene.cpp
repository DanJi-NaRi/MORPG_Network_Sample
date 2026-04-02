#include "pch.h"

#include "TestScene.h"

#include <algorithm>
#include <cmath>
#include "Player.h"
#include "ObjectManager.h"
#include "Terrain.h"
#include "UIManager.h"
#include "WorldPlayerState.h"
#include "YunoEngine.h"

bool TestScene::OnCreateScene()
{
    m_objectManager->CreateDirLight();

    Terrain* terrain = m_objectManager->CreateObject<Terrain>(
        L"Terrain_Main",
        XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (terrain && terrain->BuildStaticPlaneMesh(64, 64, 60.0f))
    {
        terrain->SetMaskColor(XMFLOAT4(0.1f, 0.3f, 0.10f, 1.0f));
    }

    return true;
}

void TestScene::OnDestroyScene()
{
    m_playerVisuals.clear();
}

void TestScene::OnEnter()
{
}

void TestScene::OnExit()
{
}

void TestScene::Update(float dt)
{
    std::vector<yuno::game::WorldEntityState> snapshotEntities;
    if (yuno::game::ConsumeWorldSnapshot(snapshotEntities))
    {
        std::uint32_t localEntityId = 0;
        const bool hasLocalEntity = yuno::game::TryGetLocalPlayerEntityId(localEntityId);

        std::unordered_map<std::uint32_t, bool> aliveMap;
        aliveMap.reserve(snapshotEntities.size());

        for (const auto& entity : snapshotEntities)
        {
            PlayerVisualRuntime* runtime = EnsurePlayerVisual(entity.entityId, entity.x, entity.y, entity.z);
            if (runtime && runtime->visual)
            {
                runtime->targetPos = XMFLOAT3(entity.x, entity.y, entity.z);
                runtime->isLocal = hasLocalEntity && (entity.entityId == localEntityId);

                if (!runtime->initialized)
                {
                    runtime->renderPos = runtime->targetPos;
                    runtime->visual->SetPos(runtime->renderPos);
                    runtime->initialized = true;
                }
            }

            aliveMap[entity.entityId] = true;
        }

        std::vector<std::uint32_t> toRemove;
        for (const auto& kv : m_playerVisuals)
        {
            if (aliveMap.find(kv.first) == aliveMap.end())
            {
                toRemove.push_back(kv.first);
            }
        }

        for (const std::uint32_t entityId : toRemove)
        {
            auto it = m_playerVisuals.find(entityId);
            if (it == m_playerVisuals.end())
                continue;

            Player* visual = it->second.visual;
            if (visual)
            {
                m_objectManager->DestroyObject(visual->GetID());
            }
            m_playerVisuals.erase(it);
        }
    }

    std::int16_t localMoveX = 0;
    std::int16_t localMoveY = 0;
    yuno::game::GetLocalInputAxis(localMoveX, localMoveY);

    for (auto& kv : m_playerVisuals)
    {
        PlayerVisualRuntime& runtime = kv.second;
        if (!runtime.visual || !runtime.initialized)
            continue;

        if (runtime.isLocal)
        {
            float inputX = static_cast<float>(localMoveX);
            float inputZ = static_cast<float>(localMoveY);
            const float inputLenSq = inputX * inputX + inputZ * inputZ;
            if (inputLenSq > 0.0f)
            {
                const float invLen = 1.0f / std::sqrt(inputLenSq);
                inputX *= invLen;
                inputZ *= invLen;

                runtime.renderPos.x += inputX * kLocalPredictionSpeed * dt;
                runtime.renderPos.z += inputZ * kLocalPredictionSpeed * dt;
            }

            const float alpha = std::min(1.0f, kLocalCorrectionRate * dt);
            runtime.renderPos.x += (runtime.targetPos.x - runtime.renderPos.x) * alpha;
            runtime.renderPos.y += (runtime.targetPos.y - runtime.renderPos.y) * alpha;
            runtime.renderPos.z += (runtime.targetPos.z - runtime.renderPos.z) * alpha;
        }
        else
        {
            const float alpha = std::min(1.0f, kRemoteInterpolationRate * dt);
            runtime.renderPos.x += (runtime.targetPos.x - runtime.renderPos.x) * alpha;
            runtime.renderPos.y += (runtime.targetPos.y - runtime.renderPos.y) * alpha;
            runtime.renderPos.z += (runtime.targetPos.z - runtime.renderPos.z) * alpha;
        }

        runtime.visual->SetPos(runtime.renderPos);
    }

    SceneBase::Update(dt);
}

void TestScene::SubmitObj()
{
    SceneBase::SubmitObj();
}

void TestScene::SubmitUI()
{
    SceneBase::SubmitUI();
}

TestScene::PlayerVisualRuntime* TestScene::EnsurePlayerVisual(std::uint32_t entityId, float x, float y, float z)
{
    if (entityId == 0)
        return nullptr;

    auto it = m_playerVisuals.find(entityId);
    if (it != m_playerVisuals.end())
    {
        return &it->second;
    }

    const std::wstring name = L"Player_" + std::to_wstring(entityId);
    Player* created = m_objectManager->CreateObjectFromFile<Player>(
        name,
        XMFLOAT3(x, y, z),
        L"../Assets/fbx/weapon/Blaster/Blaster.fbx");

    if (created)
    {
        created->SetScale(XMFLOAT3(3.0f, 3.0f, 3.0f));
        PlayerVisualRuntime runtime{};
        runtime.visual = created;
        runtime.renderPos = XMFLOAT3(x, y, z);
        runtime.targetPos = XMFLOAT3(x, y, z);
        runtime.initialized = true;
        m_playerVisuals.emplace(entityId, runtime);
        return &m_playerVisuals.find(entityId)->second;
    }

    return nullptr;
}
