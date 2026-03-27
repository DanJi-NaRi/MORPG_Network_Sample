#include "pch.h"

// 본인 씬 최상단 ㄱㄱ
#include "TestScene.h"
// 다음 엔진
#include "YunoEngine.h"
// 다음 오브젝트 매니저 여기까지 고정
#include "ObjectManager.h"
// 게임 매니저

#include "UIManager.h"
// 여러 오브젝트들 ;; 
#include "Building.h"


// 사용법
// 컨트롤 + H 누르면 이름 변경 나옴
// 위쪽 칸에 TestScene 적고
// 아래 칸에 원하는 씬 이름 적고
// 모두 바꾸기 누르면 됨 .h 파일도 동일하게 ㄱㄱ

bool TestScene::OnCreateScene()
{
    //std::cout << "[TestScene] OnCreate\n";

    // 테스트 씬 기본 광원
    m_objectManager->CreateDirLight();
    // 직교투영 필요한 씬만 ㄱㄱ
    //m_uiManager->SetOrthoFlag(true);

    // Sample Object 생성 예시
    //m_objectManager->CreateObject<Building>(L"김범진", XMFLOAT3(0, 0, 0));

    // FBX 파일로부터 오브젝트 생성 예시
    Building* obj = m_objectManager->CreateObjectFromFile<Building>(
        L"김범진",
        XMFLOAT3(0, 0, 0),
        L"../Assets/fbx/weapon/Blaster/Blaster.fbx");

    if (obj)
    {
        // 무기 단일 메시는 스케일이 작아 기본 카메라에서 거의 안 보일 수 있음
        obj->SetScale(XMFLOAT3(3.0f, 3.0f, 3.0f));
    }

    return true;
}

void TestScene::OnDestroyScene()
{
    //std::cout << "[TestScene] OnDestroy\n";

}

void TestScene::OnEnter()
{
    //std::cout << "[TestScene] OnEnter\n"; 
}

void TestScene::OnExit()
{
    //std::cout << "[TestScene] OnExit\n"; 
}

void TestScene::Update(float dt)
{
    // 이거만 있으면 오브젝트 업데이트 됨 따로 업뎃 ㄴㄴ
    SceneBase::Update(dt);
}

void TestScene::SubmitObj()
{
    // 이거만 있으면 오브젝트 렌더 됨 따로 서브밋 ㄴㄴ
    SceneBase::SubmitObj();
}

void TestScene::SubmitUI()
{
    // 이거만 있으면 오브젝트 렌더 됨 따로 서브밋 ㄴㄴ
    SceneBase::SubmitUI();
}
