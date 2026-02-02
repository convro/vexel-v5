#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <algorithm>

struct ProcessInfo {
    DWORD       pid;
    std::wstring name;
    std::wstring windowTitle;
};

inline std::vector<ProcessInfo> EnumVisibleWindows() {
    std::vector<ProcessInfo> results;

    struct Ctx { std::vector<ProcessInfo>* r; };
    Ctx ctx{ &results };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[512];
        GetWindowTextW(hwnd, title, 512);
        std::wstring t(title);
        if (t.empty()) return TRUE;
        // Skip tiny/tool windows
        LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        if (!(style & WS_CAPTION)) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        auto* ctx = reinterpret_cast<Ctx*>(lp);
        ctx->r->push_back({ pid, L"", t });
        return TRUE;
    }, (LPARAM)&ctx);

    // Fill process names
    for (auto& p : results) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    if (pe.th32ProcessID == p.pid) {
                        p.name = pe.szExeFile;
                        break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
    }
    return results;
}

inline std::wstring VkToString(int vk) {
    if (vk == 0) return L"None";
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk) {
    case VK_SPACE:  return L"Space";
    case VK_TAB:    return L"Tab";
    case VK_SHIFT:  return L"Shift";
    case VK_CONTROL:return L"Ctrl";
    case VK_MENU:   return L"Alt";
    case VK_CAPITAL:return L"CapsLock";
    case VK_ESCAPE: return L"Esc";
    case VK_OEM_3:  return L"`";
    default: {
        wchar_t buf[32];
        UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        if (GetKeyNameTextW(sc << 16, buf, 32) > 0) return buf;
        return L"Key(" + std::to_wstring(vk) + L")";
    }
    }
}

inline bool IsTargetFocused(DWORD targetPID) {
    if (targetPID == 0) return false;
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    return fgPid == targetPID;
}
