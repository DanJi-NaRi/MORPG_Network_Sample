#include "pch.h"


#include "RenderTypes.h"
#include "IInput.h"
#include "IRenderer.h"
#include "IWindow.h"
#include "YunoEngine.h"
#include "ISceneManager.h"
#include "YunoCamera.h"

#include "Widget.h"

#include"TestScene.h"


#include "AudioQueue.h"




#include "GameApp.h"

#include "PacketBuilder.h"
#include "WorldPlayerState.h"
#include "utilityClass.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <physx/PxPhysicsAPI.h>

// ???땅

#include "C2S_Ping.h"
#include "C2S_EnterWorld.h"
#include "C2S_MoveInput.h"
//#include "C2S_MatchEnter.h"
//#include "C2S_MatchLeave.h"

namespace
{
    constexpr std::uint32_t kBlasterCharacterId = 1001;
    constexpr float kMousePickMaxDistance = 10000.0f;
    constexpr float kMousePickGroundHalfExtent = 4096.0f;
    constexpr float kMousePickGroundHalfHeight = 0.1f;

    std::string ReadEnvOrDefault(const char* name, const char* fallback)
    {
        if (!name || !(*name))
            return fallback ? std::string(fallback) : std::string();

        char* buffer = nullptr;
        std::size_t size = 0;
        const errno_t ec = _dupenv_s(&buffer, &size, name);
        if (ec != 0 || !buffer)
            return fallback ? std::string(fallback) : std::string();

        std::string value(buffer);
        std::free(buffer);
        if (value.empty())
            return fallback ? std::string(fallback) : std::string();

        return value;
    }
}





GameApp::~GameApp() = default;

bool GameApp::OnInit()
{
    std::cout << "[GameApp] OnInit\n";

    IWindow* window = YunoEngine::GetWindow();
    if (window)
    {
        window->InitializeCursor(
            L"../Assets/UI/cursor/mousecursor_mouseout.cur",
            L"../Assets/UI/cursor/mousecursor_mouseclick.cur"
        );
    }

    IRenderer* renderer = YunoEngine::GetRenderer();
    if (!renderer)
    {
        std::cout << "[GameApp] Renderer not available.\n";
        return false;
    } // ???쐭??筌ｋ똾寃?


   ISceneManager* sm = YunoEngine::GetSceneManager();
   if (!sm) return false;
   
   //m_gameManager = std::make_unique<GameManager>();
   //GameManager::Initialize(m_gameManager.get());
   //m_gameManager->BindClientNetwork(&m_clientNet);
   //GameManager::Get().Init();

   SceneTransitionOptions opt{};
   opt.immediate = true;
    

   //sm->RequestReplaceRoot(std::make_unique<RenderTest>(), opt);  // 癰귣챷????臾믩씜 餓λ쵐?????앮에??대Ŋ猿?
   //sm->RequestReplaceRoot(std::make_unique<UIScene>(), opt);
   //sm->RequestReplaceRoot(std::make_unique<WeaponSelectScene>(), opt);

   //sm->RequestReplaceRoot(std::make_unique<PlayMidScene>(), opt);
   sm->RequestReplaceRoot(std::make_unique<TestScene>(), opt); 
   /*{
       sm->RequestReplaceRoot(std::make_unique<PlayScene>(), opt);
       sm->RequestPush(std::make_unique<PlayHUDScene>());
   }*/
   //sm->RequestReplaceRoot(std::make_unique<PhaseScene>(), opt);

   // UI ??沅??뱀뒠 ?묒눖諭???밴쉐
   SetupDefWidgetMesh(g_defaultWidgetMesh, renderer);

    // ??쎈뱜??곌쾿 ??살쟿????뽰삂
   const std::string serverHost = ReadServerHostFromEnv();
   const std::uint16_t serverPort = ReadServerPortFromEnv();
   std::cout << "[GameApp] Connect target=" << serverHost << ":" << serverPort << "\n";
   m_clientNet.Start(serverHost, serverPort);
    //m_clientNet.Start("127.0.0.1", 9000);

   m_clientNet.RegisterMatchPacketHandler();

    return true;
}

void GameApp::OnUpdate(float dt)
{
    //m_gameManager->Tick(dt);
    m_clientNet.PumpIncoming(dt);

    IRenderer* renderer = YunoEngine::GetRenderer();
    IInput* input = YunoEngine::GetInput();
    IWindow* window = YunoEngine::GetWindow();
    ISceneManager* sm = YunoEngine::GetSceneManager();
    IAudioManager* am = YunoEngine::GetAudioManager();

    if (!renderer || !input || !window || !sm || !am)
    {
        return;
    }

    if (m_clientNet.IsConnected() && !m_enterWorldRequested)
    {
        using namespace yuno::net;

        yuno::net::packets::C2S_EnterWorld enterWorld{};
        enterWorld.characterId = kBlasterCharacterId;
        enterWorld.spawnRegionId = 0;
        enterWorld.loginToken = ReadEnvOrDefault("YUNO_LOGIN_TOKEN", "");

        auto bytes = PacketBuilder::Build(
            PacketType::C2S_EnterWorld,
            [&enterWorld](ByteWriter& w)
            {
                enterWorld.Serialize(w);
            });

        m_clientNet.SendPacket(std::move(bytes));
        m_enterWorldRequested = true;

        std::cout << "[GameApp] enter-world requested with Blaster characterId="
            << kBlasterCharacterId << "\n";
    }

    static float acc = 0.0f;
    static int frameCount = 0;

    acc += dt;
    ++frameCount;

    CameraMove(dt);

    // MSAA 癰궰野????뮞??
    //static float test = 0.0f;
    //test += dt;
    //
    //if (test >= 4.0f) 
    //{
    //    IRenderer* renderer = YunoEngine::GetRenderer();
    //    int msaaVal = renderer->GetMSAASamples();
    //    std::cout <<"before msaa Value = " << msaaVal;
    //    if(msaaVal == 1)
    //        renderer->SetMSAASamples(8);
    //    else
    //        renderer->SetMSAASamples(1);
    //
    //    msaaVal = renderer->GetMSAASamples();
    //    std::cout << ", After msaa Value = " << msaaVal<<std::endl;
    //
    //    test = 0.0f;
    //}
    if (input->IsKeyDown('I')) // ???뮞?紐꾩뒠 ??곴맒????ν뀧??
        window->SetClientSize(960, 540);

    if (input->IsKeyDown('O')) // ???뮞?紐꾩뒠 ??곴맒????ν뀧??
        window->SetClientSize(1920, 1080);

    if (input->IsKeyDown('P'))
        window->SetClientSize(3440, 1440);


    if (input && sm)
    {
        
        //if (input->IsKeyPressed(VK_F2))
        //{
        //    SceneTransitionOptions opt{};
        //    opt.immediate = true;
        //    sm->RequestReplaceRoot(std::make_unique<PlayScene>(), opt);
        //    //sm->RequestPush(std::make_unique<PlayScene>());
        //}

        //if (input->IsKeyPressed(VK_F3))
        //{
        //    SceneTransitionOptions opt{};
        //    opt.immediate = true;
        //    sm->RequestReplaceRoot(std::make_unique<UIScene>(), opt);
        //    //sm->RequestPush(std::make_unique<PlayScene>());
        //}
        //
        //if (input->IsKeyPressed(VK_F4))
        //{
        //    SceneTransitionOptions opt{};
        //    opt.immediate = true;
        //    ScenePolicy policy;
        //    policy.blockUpdateBelow = false;
        //    policy.blockRenderBelow = false;
        //    //sm->RequestReplaceRoot(std::make_unique<TitleScene>(), opt);
        //    sm->RequestPush(std::make_unique<TitleScene>(), policy);
        //}
        //
        //if (input->IsKeyPressed(VK_F5))
        //{
        //    SceneTransitionOptions opt{};
        //    opt.immediate = true;
        //    ScenePolicy policy;
        //    policy.blockUpdateBelow = false;
        //    policy.blockRenderBelow = false;
        //    //sm->RequestReplaceRoot(std::make_unique<PlayScene>(), opt);
        //    sm->RequestPush(std::make_unique<PlayScene>(), policy);
        //}
        //
        //if (input->IsKeyPressed(VK_F6))
        //{
        //    SceneTransitionOptions opt{};
        //    opt.immediate = true;
        //    ScenePolicy policy;
        //    policy.blockUpdateBelow = false;
        //    policy.blockRenderBelow = false;
        //    //sm->RequestReplaceRoot(std::make_unique<PlayScene>(), opt);
        //    sm->RequestPush(std::make_unique<UIScene>(), policy);
        //}

        //if (input->IsKeyPressed(VK_OEM_PERIOD))
        //{
        //    AudioQ::Insert(AudioQ::StopOrRestartEvent(EventName::BGM_Playlist, true));
        //    AudioQ::Insert(AudioQ::StopOrRestartEvent(EventName::BGM_Playlist, false));
        //}

        if (input->IsKeyPressed('1'))
        {
            using namespace yuno::net;
            yuno::net::packets::C2S_Ping ping{};
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            ping.reqTime = static_cast<std::uint32_t>(ms & 0xFFFFFFFFull);

            auto bytes = PacketBuilder::Build(
                PacketType::C2S_Ping,
                [&](ByteWriter& w)
                {
                    ping.Serialize(w);
                });

            m_clientNet.SendPacket(std::move(bytes));
        }

        std::uint32_t localEntityId = 0;
        const bool hasLocalPlayer = yuno::game::TryGetLocalPlayerEntityId(localEntityId);
        if (hasLocalPlayer)
        {
            if (input->IsMouseButtonPressed(0))
            {
                float hitX = 0.0f;
                float hitY = 0.0f;
                float hitZ = 0.0f;
                if (TryPickGroundPointFromMouse(hitX, hitY, hitZ))
                {
                    m_clickMoveActive = true;
                    m_clickMoveTargetX = hitX;
                    m_clickMoveTargetZ = hitZ;
                }
            }

            float moveX = 0.0f;
            float moveY = 0.0f;

            if (input->IsKeyDown(VK_LEFT))
                moveX -= 1.0f;
            if (input->IsKeyDown(VK_RIGHT))
                moveX += 1.0f;
            if (input->IsKeyDown(VK_UP))
                moveY += 1.0f;
            if (input->IsKeyDown(VK_DOWN))
                moveY -= 1.0f;

            const bool hasManualMoveInput = (std::abs(moveX) > 0.001f || std::abs(moveY) > 0.001f);
            if (hasManualMoveInput)
            {
                m_clickMoveActive = false;
            }
            else
            {
                ApplyClickMoveToInput(moveX, moveY);
            }

            m_inputSendAccumulator += dt;
            while (m_inputSendAccumulator >= kInputSendIntervalSeconds)
            {
                m_inputSendAccumulator -= kInputSendIntervalSeconds;

                yuno::net::packets::C2S_MoveInput moveInput{};
                moveInput.entityId = localEntityId;

                yuno::net::packets::MoveInputFrame frame{};
                frame.clientTick = ++m_moveClientTick;
                frame.sequence = ++m_moveSequence;
                frame.moveX = moveX;
                frame.moveY = moveY;
                frame.buttons = 0;
                moveInput.frames.push_back(frame);

                auto moveBytes = yuno::net::PacketBuilder::Build(
                    yuno::net::PacketType::C2S_MoveInput,
                    [&moveInput](yuno::net::ByteWriter& w)
                    {
                        moveInput.Serialize(w);
                    });

                m_clientNet.SendPacket(std::move(moveBytes));
                yuno::game::PublishLocalInput(frame.sequence, frame.moveX, frame.moveY);
            }
        }
        else
        {
            m_inputSendAccumulator = 0.0f;
            m_clickMoveActive = false;
        }

    }

    // audio-> StateCheck();



    if (acc >= 1.0f)
    {
        const float fps = static_cast<float>(frameCount) / acc;
        const auto netInfo = m_clientNet.GetSnapshotAckDebugInfo();
        const std::uint32_t ackGapSnapshots =
            (netInfo.lastReceivedSnapshotId >= netInfo.lastServerSeenAckSnapshotId)
            ? (netInfo.lastReceivedSnapshotId - netInfo.lastServerSeenAckSnapshotId)
            : 0;

        std::cout << "[GameApp] FPS=" << fps
            << " Snapshot(recv=" << netInfo.lastReceivedSnapshotId
            << " ackSent=" << netInfo.lastSentAckSnapshotId
            << " serverAck=" << netInfo.lastServerSeenAckSnapshotId
            << " gap=" << ackGapSnapshots << ")\n";
        acc = 0.0f;
        frameCount = 0;
    }



    am->Update(dt);
}

bool GameApp::TryPickGroundPointFromMouse(float& outX, float& outY, float& outZ) const
{
    IRenderer* renderer = YunoEngine::GetRenderer();
    IInput* input = YunoEngine::GetInput();
    IWindow* window = YunoEngine::GetWindow();
    if (!renderer || !input || !window)
        return false;

    const float width = static_cast<float>(window->GetClientWidth());
    const float height = static_cast<float>(window->GetClientHeight());
    if (width <= 0.0f || height <= 0.0f)
        return false;

    YunoCamera& camera = renderer->GetCamera();
    const float mouseX = input->GetMouseX();
    const float mouseY = input->GetMouseY();

    const float ndcX = (mouseX / width) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (mouseY / height) * 2.0f;

    const DirectX::XMMATRIX view = camera.View();
    const DirectX::XMMATRIX proj = camera.ProjPerspective();
    const DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(nullptr, view * proj);

    const DirectX::XMVECTOR nearPoint = DirectX::XMVector3TransformCoord(
        DirectX::XMVectorSet(ndcX, ndcY, 0.0f, 1.0f),
        invViewProj);
    const DirectX::XMVECTOR farPoint = DirectX::XMVector3TransformCoord(
        DirectX::XMVectorSet(ndcX, ndcY, 1.0f, 1.0f),
        invViewProj);
    const DirectX::XMVECTOR rayDirVec = DirectX::XMVector3Normalize(farPoint - nearPoint);

    DirectX::XMFLOAT3 rayOriginFloat = camera.position;
    DirectX::XMFLOAT3 rayDirFloat{};
    DirectX::XMStoreFloat3(&rayDirFloat, rayDirVec);

    const physx::PxVec3 rayOrigin(rayOriginFloat.x, rayOriginFloat.y, rayOriginFloat.z);
    const physx::PxVec3 rayDir(rayDirFloat.x, rayDirFloat.y, rayDirFloat.z);
    if (!rayDir.isFinite())
        return false;

    const physx::PxBoxGeometry groundGeometry(
        kMousePickGroundHalfExtent,
        kMousePickGroundHalfHeight,
        kMousePickGroundHalfExtent);
    const physx::PxTransform groundPose(physx::PxVec3(0.0f, -kMousePickGroundHalfHeight, 0.0f));

    physx::PxRaycastHit hit{};
    const physx::PxU32 hitCount = physx::PxGeometryQuery::raycast(
        rayOrigin,
        rayDir,
        groundGeometry,
        groundPose,
        kMousePickMaxDistance,
        physx::PxHitFlag::ePOSITION,
        1,
        &hit);

    if (hitCount == 0)
        return false;

    outX = hit.position.x;
    outY = hit.position.y;
    outZ = hit.position.z;
    return true;
}

void GameApp::ApplyClickMoveToInput(float& inOutMoveX, float& inOutMoveY)
{
    if (!m_clickMoveActive)
        return;

    float localX = 0.0f;
    float localY = 0.0f;
    float localZ = 0.0f;
    if (!yuno::game::TryGetReconciledLocalPosition(localX, localY, localZ))
        return;

    const float deltaX = m_clickMoveTargetX - localX;
    const float deltaZ = m_clickMoveTargetZ - localZ;
    const float distanceSq = deltaX * deltaX + deltaZ * deltaZ;
    const float stopDistanceSq = kClickMoveStopDistance * kClickMoveStopDistance;
    if (distanceSq <= stopDistanceSq)
    {
        m_clickMoveActive = false;
        inOutMoveX = 0.0f;
        inOutMoveY = 0.0f;
        return;
    }

    const float distance = std::sqrt(distanceSq);
    if (distance <= 0.0001f)
    {
        m_clickMoveActive = false;
        inOutMoveX = 0.0f;
        inOutMoveY = 0.0f;
        return;
    }

    inOutMoveX = deltaX / distance;
    inOutMoveY = deltaZ / distance;

    if (std::abs(deltaX) <= kClickMoveAxisDeadZone)
        inOutMoveX = 0.0f;
    if (std::abs(deltaZ) <= kClickMoveAxisDeadZone)
        inOutMoveY = 0.0f;
}

void GameApp::OnFixedUpdate(float fixedDt)
{
    static int step = 0;
    ++step;

    //if (step % 60 == 0)
    //{
    //    std::cout << "[GameApp] FixedUpdate dt = " << fixedDt << "\n";
    //}
}

void GameApp::OnShutdown()
{
    std::cout << "[GameApp] OnShutdown\n";

    //GameManager::Shutdown();
    //m_gameManager.reset();

    // ??쎈뱜??곌쾿 ??살쟿???ル굝利?
    m_clientNet.Stop();

    //if (m_net)
    //{
    //    m_net->Close();
    //    m_net.reset();
    //}
}


void CameraMove(float dt)
{
    IRenderer* renderer = YunoEngine::GetRenderer();
    IInput* input = YunoEngine::GetInput();

    static bool s_cameraInitialized = false;
    static float s_cameraYaw = 0.0f;
    static float s_cameraPitch = 0.0f;

    YunoCamera& camera = renderer->GetCamera();
    XMVECTOR position = XMLoadFloat3(&camera.position);

    if (!s_cameraInitialized)
    {
        XMVECTOR toTarget = XMVectorSubtract(XMLoadFloat3(&camera.target), position);
        float lenSq = XMVectorGetX(XMVector3LengthSq(toTarget));
        if (lenSq < 0.0001f)
        {
            toTarget = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        XMVECTOR forward = XMVector3Normalize(toTarget);
        XMFLOAT3 forwardFloat{};
        XMStoreFloat3(&forwardFloat, forward);
        s_cameraYaw = atan2f(forwardFloat.x, forwardFloat.z);
        s_cameraPitch = asinf(forwardFloat.y);
        s_cameraInitialized = true;
    }

    const float lookSpeed = 0.005f;
    if (input->IsMouseButtonDown(1))
    {
        s_cameraYaw += input->GetMouseDeltaX() * lookSpeed;
        s_cameraPitch -= input->GetMouseDeltaY() * lookSpeed;

        const float maxPitch = XM_PIDIV2 - 0.01f;
        if (s_cameraPitch > maxPitch) s_cameraPitch = maxPitch;
        if (s_cameraPitch < -maxPitch) s_cameraPitch = -maxPitch;
    }

    XMVECTOR forward = XMVectorSet(
        cosf(s_cameraPitch) * sinf(s_cameraYaw),
        sinf(s_cameraPitch),
        cosf(s_cameraPitch) * cosf(s_cameraYaw),
        0.0f
    );
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));

    XMVECTOR move = XMVectorZero();
    if (input->IsKeyDown('W')) move = XMVectorAdd(move, forward);
    if (input->IsKeyDown('S')) move = XMVectorSubtract(move, forward);
    if (input->IsKeyDown('A')) move = XMVectorSubtract(move, right);
    if (input->IsKeyDown('D')) move = XMVectorAdd(move, right);
    if (input->IsKeyDown('Q')) move = XMVectorSubtract(move, worldUp);
    if (input->IsKeyDown('E')) move = XMVectorAdd(move, worldUp);

    const float moveSpeed = 10.0f;
    float moveLenSq = XMVectorGetX(XMVector3LengthSq(move));
    if (moveLenSq > 0.0001f)
    {
        move = XMVector3Normalize(move);
        position = XMVectorAdd(position, XMVectorScale(move, moveSpeed * dt));
    }

    XMVECTOR target = XMVectorAdd(position, forward);
    XMStoreFloat3(&camera.position, position);
    XMStoreFloat3(&camera.target, target);
    camera.up = { 0.0f, 1.0f, 0.0f };
}



