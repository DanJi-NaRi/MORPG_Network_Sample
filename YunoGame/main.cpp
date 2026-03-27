#include "pch.h"

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "YunoEngine.h"
#include "GameApp.h"

#include <string>
#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

#ifdef _DEBUG
    // Enable CRT leak check in debug.
    int dbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    dbgFlag |= _CRTDBG_ALLOC_MEM_DF;
    dbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(dbgFlag);
#endif

    std::string loginId;
    std::string loginPw;
    std::cout << u8"[로그인] ID 입력 (엔터=건너뜀): ";
    std::getline(std::cin, loginId);

    if (!loginId.empty())
    {
        std::cout << u8"[로그인] PW 입력: ";
        std::getline(std::cin, loginPw);
        _putenv_s("YUNO_LOGIN_ID", loginId.c_str());
        _putenv_s("YUNO_LOGIN_PW", loginPw.c_str());
        std::cout << u8"[로그인] 저장 완료. 서버 연결 후 자동 로그인 요청을 보냅니다.\n";
    }

    GameApp game;
    YunoEngine engine;

    std::cout << "[CWD] " << std::filesystem::current_path().string() << "\n";
    if (!engine.Initialize(&game, L"YunoGame", 1920, 1080))
    {
        return -1;
    }

    return engine.Run();
}
