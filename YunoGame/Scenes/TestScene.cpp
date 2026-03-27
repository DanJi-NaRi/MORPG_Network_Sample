#include "pch.h"

#include "TestScene.h"

#include "Building.h"
#include "ObjectManager.h"
#include "Terrain.h"
#include "UIManager.h"
#include "YunoEngine.h"

bool TestScene::OnCreateScene()
{
    m_objectManager->CreateDirLight();

    Terrain* terrain = m_objectManager->CreateObject<Terrain>(
        L"Terrain_Main",
        XMFLOAT3(0.0f, -0.5f, 0.0f));

    if (terrain && terrain->BuildStaticPlaneMesh(64, 64, 60.0f))
    {
        terrain->SetMaskColor(XMFLOAT4(0.42f, 0.45f, 0.40f, 1.0f));
    }

    Building* blaster = m_objectManager->CreateObjectFromFile<Building>(
        L"Blaster",
        XMFLOAT3(0.0f, 1.0f, 0.0f),
        L"../Assets/fbx/weapon/Blaster/Blaster.fbx");

    if (blaster)
    {
        blaster->SetScale(XMFLOAT3(3.0f, 3.0f, 3.0f));
    }

    return true;
}

void TestScene::OnDestroyScene()
{
}

void TestScene::OnEnter()
{
}

void TestScene::OnExit()
{
}

void TestScene::Update(float dt)
{
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
