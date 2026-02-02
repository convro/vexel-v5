#pragma once
#include <string>
#include <Windows.h>

struct ClickSection {
    float cps          = 10.0f;
    int   bindKey      = 0;        // virtual key code, 0 = unset
    bool  jitter       = false;
    int   mode         = 0;        // 0 = basic, 1 = pro
    bool  active       = false;    // clicker currently on
    bool  waitingBind  = false;    // waiting for user to press a key
    float randomizer   = 0.0f;    // 0-100%, CPS variance range
};

struct AppConfig {
    std::wstring username;
    bool         firstRun      = true;
    DWORD        targetPID     = 0;
    std::wstring targetName;

    ClickSection left;
    ClickSection right;

    // Auto Guard (right-clicks during left clicking)
    bool  autoGuard       = false;
    float autoGuardRate   = 20.0f;  // 0-100% chance per click cycle

    // Overlay
    bool  overlayEnabled  = false;
    int   overlayX        = 10;
    int   overlayY        = 10;
};
