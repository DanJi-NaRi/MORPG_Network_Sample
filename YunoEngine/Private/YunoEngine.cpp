#include "pch.h"

#include "YunoEngine.h"

// ?명꽣?섏씠??
#include "IGameApp.h"
//#include "IWindow.h"
//#include "IRenderer.h"
//#include "ITime.h"
//#include "IInput.h"
//#include "ISceneManager.h"

// ?좊끂
#include "YunoWindow.h"
#include "YunoRenderer.h"
#include "YunoTimer.h"
#include "YunoTextureManager.h"
#include "YunoInputSystem.h"
#include "YunoSceneManager.h"

 // ?ъ슫??
#include "AudioManagerPCH.h"

#include "ImGuiManager.h"
#include "UImgui.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#include "ImGUI_Debug.h"

IRenderer* YunoEngine::s_renderer = nullptr;
ITextureManager* YunoEngine::s_textureManager = nullptr;
IInput* YunoEngine::s_input = nullptr;
IWindow* YunoEngine::s_window = nullptr;
ISceneManager* YunoEngine::s_sceneManager = nullptr;
IAudioManager* YunoEngine::s_audioManager = nullptr;

namespace
{
    void DisableProcessPowerThrottling()
    {
#if defined(PROCESS_POWER_THROTTLING_CURRENT_VERSION)
        PROCESS_POWER_THROTTLING_STATE state{};
        state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask = 0;
        (void)SetProcessInformation(
            GetCurrentProcess(),
            ProcessPowerThrottling,
            &state,
            sizeof(state));
#endif
    }
}

YunoEngine::YunoEngine() = default;
YunoEngine::~YunoEngine()
{
    Shutdown();
}

bool YunoEngine::Initialize(IGameApp* game, const wchar_t* title, uint32_t width, uint32_t height)
{
    if (!game)
        return false;

    m_game = game;

    if (timeBeginPeriod(1) == TIMERR_NOERROR)
        m_timerResolutionRaised = true;
    DisableProcessPowerThrottling();

    // ?붾㈃ ?앹꽦
    m_window = std::make_unique<YunoWindow>();          
    if (!m_window->Create(title, width, height))
        return false;
    s_window = m_window.get();

    // ?뚮뜑???앹꽦
    m_renderer = std::make_unique<YunoRenderer>();      
    if (!m_renderer->Initialize(m_window.get()))
        return false;
    s_renderer = m_renderer.get();

    // ??留ㅻ땲? ?앹꽦
    m_sceneManager = std::make_unique<YunoSceneManager>();
    s_sceneManager = m_sceneManager.get();

    // ?ъ슫??
    // ?ъ슫??留ㅻ땲? 珥덇린??
    AudioCore::Get().Init();
    // ?ㅻ뵒??留ㅻ땲? ?앹꽦
    m_audioManager = std::make_unique<AudioManager>();
    s_audioManager = m_audioManager.get();

#ifdef _DEBUG
    auto YunoSmanager = dynamic_cast<YunoSceneManager*>(m_sceneManager.get());
    YunoSmanager->RegisterDrawSceneUI();

    auto renderer = dynamic_cast<YunoRenderer*>(m_renderer.get());
    renderer->RegisterDrawUI();
#endif

    // ?명뭼 ?쒖뒪???앹꽦
    m_input = std::make_unique<YunoInputSystem>();
    s_input = m_input.get();

    // ?띿뒪爾?留ㅻ땲? ?앹꽦
    m_textureManager = std::make_unique<YunoTextureManager>(
        static_cast<YunoRenderer*>(m_renderer.get())
    );
    s_textureManager = m_textureManager.get();

    // ??대㉧ ?앹꽦
    m_timer = std::make_unique<YunoTimer>();            
    m_timer->Initialize();
    //m_timer->SetMaxDeltaSeconds(0.1f); // 理쒕? ?꾨젅???쒗븳
    m_timer->SetTimeScale(1.0f);
    m_fixedAccumulator = 0.0;

#ifdef _DEBUG
    auto yunorenderer = dynamic_cast<YunoRenderer*>(m_renderer.get());

    ImGuiManager::Initialize(static_cast<HWND>(m_window->GetNativeHandle()), yunorenderer->m_device.Get(), yunorenderer->m_context.Get());

    ImGuiManager::RegisterDraw([this]()
        {
            auto& camera = m_renderer->GetCamera();
            //UI::DrawDebugHUD(&camera.position.x, camera.GetForward().m128_f32);
            UI::DrawCameraTransformController(&camera.position.x, &camera.target.x, 0.001);

            float fovYDeg = camera.GetFovYDegrees();
            const int changedMask = UI::DrawCameraFovController(&fovYDeg);

            if (changedMask & 1)
            {
                camera.SetFovYDegrees(fovYDeg);
            }
        }
    );
#endif

    // Game 珥덇린??
    if (!m_game->OnInit())
        return false;

    m_running = true;

    return true;
}

int YunoEngine::Run()
{
    if (!m_running || !m_window || !m_game)
        return -1;


    while (m_running)
    {
        m_input->BeginFrame();

        m_window->PollEvents(); // OS?쒗뀒 硫붿떆吏 ?꾨떖

        if (m_window->ShouldClose())    // 醫낅즺
        {
            m_running = false;
            break;
        }

        uint32_t w = 0, h = 0;
        if (m_window->ConsumeResize(w, h))      // ?붾㈃ ?ш린 蹂???덉쑝硫? (?뷀떚 ?뚮옒洹??ъ슜)
        {
            m_renderer->Resize(w, h);           // ?뚮뜑???붾㈃??媛숈씠 蹂寃?(?ㅼ솑 泥댁씤, RTV, DSV)
        }


        // ---------------------------------?낅뜲?댄듃 ?쒖옉 -----------------------------------------

        constexpr double fixedDt = 1.0 / 60.0;   // 60Hz
        constexpr int maxFixedStepsPerFrame = 5; 


        // dt 怨꾩궛
        m_timer->Tick();
        const double frameDt = static_cast<double>(m_timer->UnscaledDeltaSeconds());

        m_fixedAccumulator += frameDt;

        int steps = 0;
        while (m_fixedAccumulator >= fixedDt && steps < maxFixedStepsPerFrame)
        {
            m_game->OnFixedUpdate(static_cast<float>(fixedDt));
            m_fixedAccumulator -= fixedDt;
            ++steps;
        }

        if (steps == maxFixedStepsPerFrame)
        {
            m_fixedAccumulator = 0.0;
        }

    


        const float dt = m_timer->DeltaSeconds();
        m_game->OnUpdate(dt);

        m_input->Dispatch();
        // ???낅뜲?댄듃 (???꾪솚 ApplyPending ?ы븿)
        m_sceneManager->Update(dt);

        AudioCore::Get().Update(dt);  // ?ъ슫??

        // ---------------------------------?쒕줈???쒖옉 -----------------------------------------

        m_renderer->BeginFrame();

        m_sceneManager->SubmitAndRender(s_renderer);

        

        //s_renderer->Flush();

#ifdef _DEBUG


    //  UI 諛곗튂?섎뒗???덉븘?뚯꽌 ?쒓굅
    //    // IMGUI ?붾쾭源??ㅼ퐫??
        ImGuiManager::BeginFrame();
    
         //癒몄??좊븣 ?닿굅 ?怨?癒몄? ?긱꽦
        //if (m_sceneManager->GetActiveScene()->GetUIManager()) {
        //    auto& map = m_sceneManager->GetActiveScene()->GetUIManager()->GetWidgetlist();
        //    for (const auto& kv : map) // kv: pair<const UINT, Widget*>
        //    {
        //
        //        if (auto* cs = dynamic_cast<Slot*>(kv.second))
        //        {
        //            if(cs->IsSnapped()) DrawDebugRect_Client(cs->GetSnapPoint()->snapRange, Int3(0, 0, 255));
        //            else DrawDebugRect_Client(cs->GetSnapPoint()->snapRange, Int3(255, 0, 0));
        //            //else DrawDebugRect_Client(cs->GetRect());
        //        }
        //
        //        if (auto* cs = dynamic_cast<Widget*>(kv.second))
        //            DrawDebugRect_Client(cs->GetRect());
        //        //if (auto* cs = dynamic_cast<Image*>(kv.second))
        //        //    DrawDebugRect_Client(cs->GetRect());
        //    }
        //}
    
        
        ImGuiManager::EndFrame();
#endif

        m_renderer->EndFrame();
    }

    Shutdown();
    return 0;
}

void YunoEngine::Shutdown()
{
    // 醫낅즺 ?쒖꽌
#ifdef _DEBUG
    ImGuiManager::Shutdown();
#endif

    // 1. 寃뚯엫
    if (m_game)
    {
        m_game->OnShutdown();
        m_game = nullptr;
    }
    // 2. ?щℓ?덉?
    s_sceneManager = nullptr;
    m_sceneManager.reset();

    // 3. ?띿뒪爾?留ㅻ땲?
    s_textureManager = nullptr;
    m_textureManager.reset();

    // 4. ?뚮뜑??
    if (m_renderer)
    {
        m_renderer->Shutdown();
        s_renderer = nullptr;
        m_renderer.reset();
    }

    // 5. ??대㉧ 
    m_timer.reset();

    // 6. ?덈룄??
    s_window = nullptr;
    m_window.reset();

    // 7. ?ъ슫???쒖뒪??
    AudioCore::Get().Shutdown();

    m_running = false;

    if (m_timerResolutionRaised)
    {
        timeEndPeriod(1);
        m_timerResolutionRaised = false;
    }
}

