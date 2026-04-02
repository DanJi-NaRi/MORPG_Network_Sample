#pragma once

class IGameApp;
class IWindow;
class IRenderer;
class ITime;
class ITextureManager;
class IWindow;
class IInput;
class YunoInputSystem;
class ISceneManager;
class IAudioManager;
class YunoRenderer;


class YunoEngine
{
public:
    YunoEngine();
    ~YunoEngine();

    bool Initialize(IGameApp* game, const wchar_t* title, uint32_t width, uint32_t height);
    int Run();
    void Shutdown();

    bool IsRunning() const { return m_running; }

    static IRenderer* GetRenderer() { return s_renderer; }
    static ITextureManager* GetTextureManager() { return s_textureManager; }
    static IInput* GetInput() { return s_input; }
    static IWindow* GetWindow() { return s_window; }
    static ISceneManager* GetSceneManager() { return s_sceneManager; }
    static IAudioManager* GetAudioManager() { return s_audioManager; }

private:
    bool m_running = false;                             // ?붿쭊 ?묐룞 ?щ?
    bool m_timerResolutionRaised = false;
    IGameApp* m_game = nullptr;                         // ?붿쭊?쇰줈 ?뚮┫ 寃뚯엫
    std::unique_ptr<IWindow>    m_window;               // ?붾㈃
    std::unique_ptr<IRenderer>  m_renderer;             // ?뚮뜑??
    std::unique_ptr<ITime>      m_timer;                // ??대㉧
    std::unique_ptr<ITextureManager> m_textureManager;  // ?띿뒪爾?留ㅻ땲?
    std::unique_ptr<YunoInputSystem> m_input;           // ?명뭼 ?쒖뒪??
    std::unique_ptr<ISceneManager> m_sceneManager;      // ??留ㅻ땲?
    std::unique_ptr<IAudioManager> m_audioManager;      // ?ㅻ뵒??留ㅻ땲?
    double  m_fixedAccumulator = 0.0;                   // FixedUpdate ?꾩쟻 ?쒓컙

    static IRenderer* s_renderer;
    static ITextureManager* s_textureManager;
    static IInput* s_input;
    static IWindow* s_window;
    static ISceneManager* s_sceneManager;
    static IAudioManager* s_audioManager;
};
