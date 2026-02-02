#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#include "gui.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Init GDI+
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // Create GUI
    VexelGUI gui;
    g_gui = &gui;
    gui.Create(hInst);

    // Install low-level keyboard hook for global bind detection
    HHOOK kbHook = SetWindowsHookExW(WH_KEYBOARD_LL,
        [](int code, WPARAM wp, LPARAM lp) -> LRESULT {
            if (code == HC_ACTION && wp == WM_KEYDOWN && g_gui) {
                auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
                int vk = (int)kb->vkCode;

                // Only process binds when in Main screen and not waiting for bind
                if (g_gui->screen == Screen::Main &&
                    !g_gui->cfg.left.waitingBind &&
                    !g_gui->cfg.right.waitingBind) {

                    bool handled = false;
                    if (g_gui->cfg.left.bindKey && vk == g_gui->cfg.left.bindKey) {
                        g_gui->ToggleClicker(g_gui->cfg.left, g_gui->leftClicker, true);
                        handled = true;
                    }
                    if (g_gui->cfg.right.bindKey && vk == g_gui->cfg.right.bindKey) {
                        g_gui->ToggleClicker(g_gui->cfg.right, g_gui->rightClicker, false);
                        handled = true;
                    }
                    if (handled) {
                        InvalidateRect(g_gui->hwnd, nullptr, FALSE);
                    }
                }
            }
            return CallNextHookEx(nullptr, code, wp, lp);
        }, nullptr, 0);

    // Repaint timer for cursor blink + status updates
    SetTimer(gui.hwnd, 99, 100, nullptr);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    g_gui->leftClicker.Stop();
    g_gui->rightClicker.Stop();
    UnhookWindowsHookEx(kbHook);
    g_gui = nullptr;
    Gdiplus::GdiplusShutdown(gdipToken);
    return 0;
}
