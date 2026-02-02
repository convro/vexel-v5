#pragma once
#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <functional>
#include "theme.h"
#include "config.h"
#include "utils.h"
#include "clicker.h"

#pragma comment(lib, "gdiplus.lib")

// Forward
class VexelGUI;
inline VexelGUI* g_gui = nullptr;

// ==================== Drawing Helpers ====================

inline void FillRoundRect(Gdiplus::Graphics& g, Gdiplus::Brush& br,
    int x, int y, int w, int h, int r) {
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, r * 2, r * 2, 180, 90);
    path.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
    path.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
    path.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
    path.CloseFigure();
    g.FillPath(&br, &path);
}

inline void DrawRoundRect(Gdiplus::Graphics& g, Gdiplus::Pen& pen,
    int x, int y, int w, int h, int r) {
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, r * 2, r * 2, 180, 90);
    path.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
    path.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
    path.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
    path.CloseFigure();
    g.DrawPath(&pen, &path);
}

// ==================== Screens ====================
enum class Screen {
    Loading,
    Username,
    Main
};

// ==================== Main GUI Class ====================
class VexelGUI {
public:
    HWND        hwnd = nullptr;
    AppConfig   cfg;
    Screen      screen = Screen::Loading;

    // Loading
    float       loadProgress = 0.0f;
    int         loadTimerId = 1;

    // Username input
    std::wstring inputText;
    bool        inputFocused = true;

    // Main GUI
    int         activeTab = 0; // 0=config, 1=combat, 2=info
    std::vector<ProcessInfo> processes;
    int         selectedProcess = -1;
    bool        processConfirmed = false;

    // Combat
    Clicker     leftClicker;
    Clicker     rightClicker;

    // UI interaction state
    POINT       mousePos{};
    bool        mouseDown = false;
    bool        dragging = false;
    POINT       dragStart{};

    // Slider drag
    int         draggingSlider = -1; // -1=none, 0=leftCPS, 1=rightCPS
    RECT        sliderRects[2]{};

    // Bind waiting
    // handled via cfg.left.waitingBind / cfg.right.waitingBind

    // Scroll for process list
    int         processScroll = 0;

    // ============================================================
    void Create(HINSTANCE hInst) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"VexelV5";
        RegisterClassExW(&wc);

        // Start with loading window (smaller)
        hwnd = CreateWindowExW(
            WS_EX_LAYERED,
            L"VexelV5", L"Vexel V5",
            WS_POPUP,
            CW_USEDEFAULT, CW_USEDEFAULT,
            Theme::LoadWinW, Theme::LoadWinH,
            nullptr, nullptr, hInst, nullptr);

        SetLayeredWindowAttributes(hwnd, 0, 245, LWA_ALPHA);

        // Center
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd, nullptr,
            (sw - (rc.right - rc.left)) / 2,
            (sh - (rc.bottom - rc.top)) / 2,
            0, 0, SWP_NOSIZE | SWP_NOZORDER);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        // Start loading timer
        SetTimer(hwnd, loadTimerId, 30, nullptr);
    }

    void TransitionToUsername() {
        screen = Screen::Username;
        KillTimer(hwnd, loadTimerId);
        // Resize window
        ResizeWindow(Theme::LoadWinW, 260);
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void TransitionToMain() {
        cfg.username = inputText;
        cfg.firstRun = false;
        screen = Screen::Main;
        activeTab = 0; // config tab first
        ResizeWindow(Theme::WinW, Theme::WinH);
        // Refresh process list
        processes = EnumVisibleWindows();
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void ResizeWindow(int w, int h) {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int cx = (rc.left + rc.right) / 2;
        int cy = (rc.top + rc.bottom) / 2;
        SetWindowPos(hwnd, nullptr, cx - w / 2, cy - h / 2, w, h, SWP_NOZORDER);
    }

    // ==================== PAINT ====================
    void OnPaint(HDC hdc, RECT& rc) {
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // Background
        Gdiplus::SolidBrush bgBr(Theme::GdipBgDark());
        FillRoundRect(g, bgBr, 0, 0, w, h, Theme::Radius);

        // Border
        Gdiplus::Pen borderPen(Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, borderPen, 0, 0, w - 1, h - 1, Theme::Radius);

        switch (screen) {
        case Screen::Loading:  PaintLoading(g, w, h); break;
        case Screen::Username: PaintUsername(g, w, h); break;
        case Screen::Main:     PaintMain(g, w, h);    break;
        }

        // Blit
        Gdiplus::Graphics target(hdc);
        target.DrawImage(&bmp, 0, 0);
    }

    // ---------- Loading ----------
    void PaintLoading(Gdiplus::Graphics& g, int w, int h) {
        Gdiplus::Font titleFont(Theme::FontName, 20, Gdiplus::FontStyleBold);
        Gdiplus::Font subFont(Theme::FontName, 9, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush subBr(Theme::GdipTextSec());
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);

        // Title
        Gdiplus::RectF titleRc(0, 40, (float)w, 40);
        g.DrawString(L"VEXEL V5", -1, &titleFont, titleRc, &sf, &textBr);

        // Subtitle
        Gdiplus::RectF subRc(0, 80, (float)w, 20);
        g.DrawString(L"Loading...", -1, &subFont, subRc, &sf, &subBr);

        // Progress bar background
        int barX = 60, barY = 130, barW = w - 120, barH = 14;
        Gdiplus::SolidBrush barBg(Theme::GdipBgInput());
        FillRoundRect(g, barBg, barX, barY, barW, barH, 7);

        // Progress bar fill
        int fillW = (int)(barW * loadProgress);
        if (fillW > 4) {
            Gdiplus::LinearGradientBrush gradBr(
                Gdiplus::Point(barX, barY),
                Gdiplus::Point(barX + fillW, barY),
                Theme::GdipAccentDim(), Theme::GdipAccent());
            FillRoundRect(g, gradBr, barX, barY, fillW, barH, 7);
        }

        // Percentage
        Gdiplus::Font pctFont(Theme::FontName, 8, Gdiplus::FontStyleRegular);
        std::wstring pctStr = std::to_wstring((int)(loadProgress * 100)) + L"%";
        Gdiplus::RectF pctRc(0, (float)barY + barH + 8, (float)w, 16);
        g.DrawString(pctStr.c_str(), -1, &pctFont, pctRc, &sf, &subBr);

        // Version
        Gdiplus::RectF verRc(0, (float)h - 30, (float)w, 16);
        Gdiplus::SolidBrush mutedBr(Theme::GdipTextMuted());
        g.DrawString(L"v5.0.0", -1, &pctFont, verRc, &sf, &mutedBr);
    }

    // ---------- Username ----------
    void PaintUsername(Gdiplus::Graphics& g, int w, int h) {
        Gdiplus::Font titleFont(Theme::FontName, 16, Gdiplus::FontStyleBold);
        Gdiplus::Font labelFont(Theme::FontName, 10, Gdiplus::FontStyleRegular);
        Gdiplus::Font inputFont(Theme::FontName, 12, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush subBr(Theme::GdipTextSec());
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);

        // Title
        Gdiplus::RectF titleRc(0, 30, (float)w, 30);
        g.DrawString(L"Welcome to Vexel V5", -1, &titleFont, titleRc, &sf, &textBr);

        // Label
        Gdiplus::RectF labRc(0, 70, (float)w, 20);
        g.DrawString(L"How should we call you?", -1, &labelFont, labRc, &sf, &subBr);

        // Input box
        int inpX = 60, inpY = 105, inpW = w - 120, inpH = 36;
        Gdiplus::SolidBrush inpBg(Theme::GdipBgInput());
        FillRoundRect(g, inpBg, inpX, inpY, inpW, inpH, Theme::SmallRadius);
        Gdiplus::Pen inpBorder(inputFocused ? Theme::GdipAccent() : Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, inpBorder, inpX, inpY, inpW, inpH, Theme::SmallRadius);

        // Input text
        Gdiplus::StringFormat sfLeft;
        sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
        sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        std::wstring display = inputText;
        if (inputFocused && (GetTickCount64() / 500) % 2 == 0) display += L"|";
        Gdiplus::RectF txtRc((float)inpX + 12, (float)inpY, (float)inpW - 24, (float)inpH);
        if (inputText.empty() && !inputFocused) {
            g.DrawString(L"Your name...", -1, &inputFont, txtRc, &sfLeft, &subBr);
        } else {
            g.DrawString(display.c_str(), -1, &inputFont, txtRc, &sfLeft, &textBr);
        }

        // Button
        int btnW = 140, btnH = 36;
        int btnX = (w - btnW) / 2, btnY = 165;
        bool hover = mousePos.x >= btnX && mousePos.x <= btnX + btnW &&
                     mousePos.y >= btnY && mousePos.y <= btnY + btnH;
        Gdiplus::SolidBrush btnBr(hover ? Theme::GdipAccent() : Theme::GdipAccentDim());
        FillRoundRect(g, btnBr, btnX, btnY, btnW, btnH, Theme::SmallRadius);

        Gdiplus::Font btnFont(Theme::FontName, 11, Gdiplus::FontStyleBold);
        Gdiplus::SolidBrush btnText(Theme::GdipBgDark());
        Gdiplus::RectF btnRc((float)btnX, (float)btnY, (float)btnW, (float)btnH);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"Continue", -1, &btnFont, btnRc, &sf, &btnText);
    }

    // ---------- Main ----------
    void PaintMain(Gdiplus::Graphics& g, int w, int h) {
        // Title bar
        Gdiplus::SolidBrush panelBr(Theme::GdipBgPanel());
        FillRoundRect(g, panelBr, 0, 0, w, 36, Theme::Radius);
        // cover bottom corners of title bar
        Gdiplus::SolidBrush bgBr(Theme::GdipBgDark());
        g.FillRectangle(&panelBr, 0, 26, w, 10);

        // Title text
        Gdiplus::Font titleFont(Theme::FontName, 10, Gdiplus::FontStyleBold);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush accentBr(Theme::GdipAccent());
        Gdiplus::StringFormat sfLeft;
        sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
        sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF titleRc(14, 0, 200, 36);
        g.DrawString(L"Vexel V5", -1, &titleFont, titleRc, &sfLeft, &accentBr);

        // Username on right
        Gdiplus::Font smallFont(Theme::FontName, 8, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush secBr(Theme::GdipTextSec());
        std::wstring greeting = L"Hey, " + cfg.username;
        Gdiplus::StringFormat sfRight;
        sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
        sfRight.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF greetRc(0, 0, (float)w - 44, 36);
        g.DrawString(greeting.c_str(), -1, &smallFont, greetRc, &sfRight, &secBr);

        // Close button
        int closeX = w - 34, closeY = 8, closeS = 20;
        bool closeHover = mousePos.x >= closeX && mousePos.x <= closeX + closeS &&
                          mousePos.y >= closeY && mousePos.y <= closeY + closeS;
        if (closeHover) {
            Gdiplus::SolidBrush dangerBr(Theme::GdipDanger());
            FillRoundRect(g, dangerBr, closeX, closeY, closeS, closeS, 4);
        }
        Gdiplus::Pen xPen(Gdiplus::Color(255, 200, 200, 210), 1.5f);
        g.DrawLine(&xPen, closeX + 5, closeY + 5, closeX + closeS - 5, closeY + closeS - 5);
        g.DrawLine(&xPen, closeX + closeS - 5, closeY + 5, closeX + 5, closeY + closeS - 5);

        // Tab bar
        int tabY = 36;
        Gdiplus::SolidBrush tabBg(Theme::GdipBgPanel());
        g.FillRectangle(&tabBg, 0, tabY, w, Theme::TabBarH);
        Gdiplus::Pen sepPen(Theme::GdipBorder(), 1.0f);
        g.DrawLine(&sepPen, 0, tabY + Theme::TabBarH, w, tabY + Theme::TabBarH);

        const wchar_t* tabNames[] = { L"Config", L"Combat", L"Info" };
        int tabCount = 3;
        int tabW = 100;
        int tabStartX = 14;
        Gdiplus::Font tabFont(Theme::FontName, 10, Gdiplus::FontStyleRegular);
        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        for (int i = 0; i < tabCount; i++) {
            int tx = tabStartX + i * (tabW + 6);
            bool isActive = (i == activeTab);
            bool isHover = mousePos.x >= tx && mousePos.x <= tx + tabW &&
                           mousePos.y >= tabY && mousePos.y <= tabY + Theme::TabBarH;

            if (isActive) {
                Gdiplus::SolidBrush activeBr(Theme::GdipBgCard());
                FillRoundRect(g, activeBr, tx, tabY + 6, tabW, Theme::TabBarH - 6, Theme::SmallRadius);
                // Active indicator line
                Gdiplus::SolidBrush indBr(Theme::GdipAccent());
                g.FillRectangle(&indBr, tx + 20, tabY + Theme::TabBarH - 3, tabW - 40, 3);
            }

            Gdiplus::RectF tabRc((float)tx, (float)tabY, (float)tabW, (float)Theme::TabBarH);
            g.DrawString(tabNames[i], -1, &tabFont, tabRc, &sfCenter,
                isActive ? &accentBr : (isHover ? &textBr : &secBr));
        }

        // Content area
        int contentY = tabY + Theme::TabBarH + 8;
        int contentH = h - contentY - 8;

        // Only allow Combat/Info if process confirmed
        if (activeTab == 0) {
            PaintConfigTab(g, 10, contentY, w - 20, contentH);
        } else if (activeTab == 1) {
            if (!processConfirmed) {
                Gdiplus::RectF msgRc(0, (float)contentY + 40, (float)w, 30);
                g.DrawString(L"Select and confirm a target window in Config first.",
                    -1, &smallFont, msgRc, &sfCenter, &secBr);
            } else {
                PaintCombatTab(g, 10, contentY, w - 20, contentH);
            }
        } else if (activeTab == 2) {
            PaintInfoTab(g, 10, contentY, w - 20, contentH);
        }

        // Status bar
        Gdiplus::RectF statRc(14, (float)h - 24, (float)w - 28, 18);
        std::wstring status = L"Target: ";
        if (cfg.targetPID == 0) status += L"None";
        else status += cfg.targetName + L" (PID " + std::to_wstring(cfg.targetPID) + L")";
        status += L"  |  L: " + std::wstring(cfg.left.active ? L"ON" : L"OFF");
        status += L"  R: " + std::wstring(cfg.right.active ? L"ON" : L"OFF");
        Gdiplus::SolidBrush mutBr(Theme::GdipTextMuted());
        Gdiplus::Font statFont(Theme::FontName, 7, Gdiplus::FontStyleRegular);
        g.DrawString(status.c_str(), -1, &statFont, statRc, &sfLeft, &mutBr);
    }

    // ---------- Config Tab ----------
    void PaintConfigTab(Gdiplus::Graphics& g, int x, int y, int w, int h) {
        Gdiplus::Font labelFont(Theme::FontName, 10, Gdiplus::FontStyleBold);
        Gdiplus::Font itemFont(Theme::FontName, 9, Gdiplus::FontStyleRegular);
        Gdiplus::Font smallFont(Theme::FontName, 8, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush secBr(Theme::GdipTextSec());
        Gdiplus::SolidBrush accentBr(Theme::GdipAccent());
        Gdiplus::StringFormat sfLeft;
        sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);

        // Section title
        Gdiplus::RectF titleRc((float)x, (float)y, (float)w, 24);
        g.DrawString(L"Select Target Window", -1, &labelFont, titleRc, &sfLeft, &textBr);

        // Refresh button
        int refX = x + w - 80, refY = y;
        bool refHover = mousePos.x >= refX && mousePos.x <= refX + 80 &&
                        mousePos.y >= refY && mousePos.y <= refY + 24;
        Gdiplus::SolidBrush refBr(refHover ? Theme::GdipAccent() : Theme::GdipAccentDim());
        FillRoundRect(g, refBr, refX, refY, 80, 24, Theme::SmallRadius);
        Gdiplus::SolidBrush btnTxt(Theme::GdipBgDark());
        Gdiplus::StringFormat sfC;
        sfC.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfC.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF refRc((float)refX, (float)refY, 80, 24);
        g.DrawString(L"Refresh", -1, &smallFont, refRc, &sfC, &btnTxt);

        // Process list
        int listY = y + 34;
        int listH = h - 80;
        Gdiplus::SolidBrush cardBr(Theme::GdipBgCard());
        FillRoundRect(g, cardBr, x, listY, w, listH, Theme::SmallRadius);
        Gdiplus::Pen borderPen(Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, borderPen, x, listY, w, listH, Theme::SmallRadius);

        if (processes.empty()) {
            Gdiplus::RectF emptyRc((float)x, (float)listY, (float)w, (float)listH);
            g.DrawString(L"No windows found. Click Refresh to scan.",
                -1, &itemFont, emptyRc, &sfC, &secBr);
        } else {
            int itemH = 36;
            // Clip to list area
            g.SetClip(Gdiplus::Rect(x + 1, listY + 1, w - 2, listH - 2));
            for (int i = 0; i < (int)processes.size(); i++) {
                int iy = listY + 6 + i * itemH - processScroll;
                if (iy + itemH < listY || iy > listY + listH) continue;

                bool isSel = (i == selectedProcess);
                bool isHov = mousePos.x >= x && mousePos.x <= x + w &&
                             mousePos.y >= iy && mousePos.y <= iy + itemH;

                if (isSel) {
                    Gdiplus::SolidBrush selBr(Gdiplus::Color(60, 0, 200, 160));
                    FillRoundRect(g, selBr, x + 4, iy, w - 8, itemH - 2, 4);
                } else if (isHov) {
                    Gdiplus::SolidBrush hovBr(Theme::GdipBgInput());
                    FillRoundRect(g, hovBr, x + 4, iy, w - 8, itemH - 2, 4);
                }

                std::wstring line = processes[i].windowTitle;
                if (line.size() > 60) line = line.substr(0, 60) + L"...";
                Gdiplus::RectF iRc((float)x + 14, (float)iy, (float)w - 28, (float)itemH);
                sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(line.c_str(), -1, &itemFont, iRc, &sfLeft,
                    isSel ? &accentBr : &textBr);

                // PID on right
                std::wstring pidStr = L"PID " + std::to_wstring(processes[i].pid);
                Gdiplus::StringFormat sfR;
                sfR.SetAlignment(Gdiplus::StringAlignmentFar);
                sfR.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(pidStr.c_str(), -1, &smallFont, iRc, &sfR, &secBr);
            }
            g.ResetClip();
        }

        // Confirm button
        int btnW = 160, btnH = 32;
        int btnX = x + (w - btnW) / 2, btnY = listY + listH + 12;
        bool canConfirm = selectedProcess >= 0;
        bool cHover = canConfirm && mousePos.x >= btnX && mousePos.x <= btnX + btnW &&
                      mousePos.y >= btnY && mousePos.y <= btnY + btnH;
        Gdiplus::SolidBrush cBr(canConfirm
            ? (cHover ? Theme::GdipAccent() : Theme::GdipAccentDim())
            : Theme::GdipBgInput());
        FillRoundRect(g, cBr, btnX, btnY, btnW, btnH, Theme::SmallRadius);
        Gdiplus::SolidBrush cTxt(canConfirm ? Theme::GdipBgDark() : Theme::GdipTextMuted());
        Gdiplus::RectF cRc((float)btnX, (float)btnY, (float)btnW, (float)btnH);
        g.DrawString(processConfirmed ? L"Confirmed" : L"Confirm Selection",
            -1, &itemFont, cRc, &sfC, &cTxt);
    }

    // ---------- Combat Tab ----------
    void PaintCombatTab(Gdiplus::Graphics& g, int x, int y, int w, int h) {
        int halfW = (w - 12) / 2;
        PaintClickSection(g, x, y, halfW, h, true,  cfg.left,  0);
        PaintClickSection(g, x + halfW + 12, y, halfW, h, false, cfg.right, 1);
    }

    void PaintClickSection(Gdiplus::Graphics& g, int x, int y, int w, int h,
                           bool isLeft, ClickSection& sec, int sliderIdx) {
        Gdiplus::Font titleFont(Theme::FontName, 11, Gdiplus::FontStyleBold);
        Gdiplus::Font labelFont(Theme::FontName, 9, Gdiplus::FontStyleRegular);
        Gdiplus::Font valueFont(Theme::FontName, 10, Gdiplus::FontStyleBold);
        Gdiplus::Font smallFont(Theme::FontName, 8, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush secBr(Theme::GdipTextSec());
        Gdiplus::SolidBrush accentBr(Theme::GdipAccent());
        Gdiplus::StringFormat sfLeft, sfCenter, sfRight;
        sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfRight.SetAlignment(Gdiplus::StringAlignmentFar);

        // Card background
        Gdiplus::SolidBrush cardBr(Theme::GdipBgCard());
        FillRoundRect(g, cardBr, x, y, w, h - 8, Theme::SmallRadius);
        Gdiplus::Pen borderPen(Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, borderPen, x, y, w, h - 8, Theme::SmallRadius);

        int pad = 14;
        int cy = y + pad;

        // Title
        std::wstring title = isLeft ? L"Left Click" : L"Right Click";
        Gdiplus::RectF tRc((float)x + pad, (float)cy, (float)w - pad * 2, 22);
        g.DrawString(title.c_str(), -1, &titleFont, tRc, &sfLeft, &accentBr);
        cy += 28;

        // Status indicator
        bool on = sec.active;
        Gdiplus::SolidBrush statusBr(on
            ? Gdiplus::Color(255, 0, 200, 100)
            : Gdiplus::Color(255, 80, 80, 80));
        g.FillEllipse(&statusBr, x + w - pad - 12, y + pad + 4, 10, 10);

        // CPS Label + value
        std::wstring cpsStr = std::to_wstring((int)sec.cps) + L" CPS";
        Gdiplus::RectF cpsLbl((float)x + pad, (float)cy, 60, 18);
        g.DrawString(L"CPS:", -1, &labelFont, cpsLbl, &sfLeft, &secBr);
        Gdiplus::RectF cpsVal((float)x + pad + 50, (float)cy, 80, 18);
        g.DrawString(cpsStr.c_str(), -1, &valueFont, cpsVal, &sfLeft, &textBr);

        // CPS input box (small, right side)
        int inpX = x + w - pad - 52, inpY2 = cy - 1, inpW2 = 48, inpH2 = 22;
        Gdiplus::SolidBrush inpBg(Theme::GdipBgInput());
        FillRoundRect(g, inpBg, inpX, inpY2, inpW2, inpH2, 4);
        Gdiplus::Pen inpBor(Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, inpBor, inpX, inpY2, inpW2, inpH2, 4);
        std::wstring cpsNum = std::to_wstring((int)sec.cps);
        Gdiplus::RectF inpRc((float)inpX, (float)inpY2, (float)inpW2, (float)inpH2);
        g.DrawString(cpsNum.c_str(), -1, &labelFont, inpRc, &sfCenter, &textBr);

        cy += 26;

        // Slider
        int slX = x + pad, slY = cy + 4, slW = w - pad * 2, slH = 8;
        sliderRects[sliderIdx] = { slX, slY - 6, slX + slW, slY + slH + 6 };

        // Track
        Gdiplus::SolidBrush trackBr(Theme::GdipBgInput());
        FillRoundRect(g, trackBr, slX, slY, slW, slH, 4);

        // Fill
        float pct = (sec.cps - 1.0f) / 49.0f; // 1-50
        if (pct < 0) pct = 0; if (pct > 1) pct = 1;
        int fillW = (int)(slW * pct);
        if (fillW > 4) {
            Gdiplus::LinearGradientBrush gradBr(
                Gdiplus::Point(slX, slY), Gdiplus::Point(slX + fillW, slY),
                Theme::GdipAccentDim(), Theme::GdipAccent());
            FillRoundRect(g, gradBr, slX, slY, fillW, slH, 4);
        }

        // Thumb
        int thumbX = slX + fillW - 7;
        if (thumbX < slX) thumbX = slX;
        Gdiplus::SolidBrush thumbBr(Theme::GdipAccentGlow());
        g.FillEllipse(&thumbBr, thumbX, slY - 3, 14, 14);

        // Range labels
        Gdiplus::RectF minRc((float)slX, (float)slY + slH + 4, 30, 14);
        g.DrawString(L"1", -1, &smallFont, minRc, &sfLeft, &secBr);
        Gdiplus::RectF maxRc((float)slX + slW - 30, (float)slY + slH + 4, 30, 14);
        g.DrawString(L"50", -1, &smallFont, maxRc, &sfRight, &secBr);

        cy += 36;

        // Mode selection
        Gdiplus::RectF modeLbl((float)x + pad, (float)cy, 50, 18);
        g.DrawString(L"Mode:", -1, &labelFont, modeLbl, &sfLeft, &secBr);
        cy += 20;

        // Mode buttons
        for (int m = 0; m < 2; m++) {
            int mx = x + pad + m * ((w - pad * 2) / 2);
            int mw = (w - pad * 2) / 2 - 4;
            bool isSel = sec.mode == m;
            bool mHov = mousePos.x >= mx && mousePos.x <= mx + mw &&
                        mousePos.y >= cy && mousePos.y <= cy + 26;
            Gdiplus::SolidBrush mBr(isSel ? Theme::GdipAccentDim()
                : (mHov ? Theme::GdipBgInput() : Theme::GdipBgPanel()));
            FillRoundRect(g, mBr, mx, cy, mw, 26, 4);
            if (isSel) {
                Gdiplus::Pen selPen(Theme::GdipAccent(), 1.0f);
                DrawRoundRect(g, selPen, mx, cy, mw, 26, 4);
            }
            Gdiplus::RectF mRc((float)mx, (float)cy, (float)mw, 26);
            g.DrawString(m == 0 ? L"Basic" : L"Pro", -1, &labelFont, mRc, &sfCenter,
                isSel ? &accentBr : &textBr);
        }
        cy += 34;

        // Jitter toggle
        bool jHov = mousePos.x >= x + pad && mousePos.x <= x + pad + w - pad * 2 &&
                    mousePos.y >= cy && mousePos.y <= cy + 26;
        Gdiplus::SolidBrush jBg(sec.jitter ? Theme::GdipAccentDim() : Theme::GdipBgPanel());
        int jw = w - pad * 2;
        FillRoundRect(g, jBg, x + pad, cy, jw, 26, 4);
        if (sec.jitter) {
            Gdiplus::Pen jp(Theme::GdipAccent(), 1.0f);
            DrawRoundRect(g, jp, x + pad, cy, jw, 26, 4);
        }
        Gdiplus::RectF jRc((float)x + pad, (float)cy, (float)jw, 26);
        std::wstring jText = L"Jitter: " + std::wstring(sec.jitter ? L"ON" : L"OFF");
        g.DrawString(jText.c_str(), -1, &labelFont, jRc, &sfCenter,
            sec.jitter ? &accentBr : &secBr);
        cy += 34;

        // Bind
        Gdiplus::RectF bLbl((float)x + pad, (float)cy, 50, 20);
        g.DrawString(L"Bind:", -1, &labelFont, bLbl, &sfLeft, &secBr);

        int bx = x + pad + 50, bw = w - pad * 2 - 50;
        bool bHov = mousePos.x >= bx && mousePos.x <= bx + bw &&
                    mousePos.y >= cy && mousePos.y <= cy + 28;
        Gdiplus::SolidBrush bBg(sec.waitingBind
            ? Gdiplus::Color(60, 0, 200, 160)
            : (bHov ? Theme::GdipBgInput() : Theme::GdipBgPanel()));
        FillRoundRect(g, bBg, bx, cy, bw, 28, 4);
        Gdiplus::Pen bPen(sec.waitingBind ? Theme::GdipAccent() : Theme::GdipBorder(), 1.0f);
        DrawRoundRect(g, bPen, bx, cy, bw, 28, 4);

        std::wstring bindStr = sec.waitingBind ? L"Press any key..."
            : (sec.bindKey ? VkToString(sec.bindKey) : L"Click to set");
        Gdiplus::RectF bRc((float)bx, (float)cy, (float)bw, 28);
        g.DrawString(bindStr.c_str(), -1, &labelFont, bRc, &sfCenter,
            sec.waitingBind ? &accentBr : &textBr);
    }

    // ---------- Info Tab ----------
    void PaintInfoTab(Gdiplus::Graphics& g, int x, int y, int w, int h) {
        Gdiplus::Font titleFont(Theme::FontName, 12, Gdiplus::FontStyleBold);
        Gdiplus::Font bodyFont(Theme::FontName, 9, Gdiplus::FontStyleRegular);
        Gdiplus::SolidBrush textBr(Theme::GdipText());
        Gdiplus::SolidBrush secBr(Theme::GdipTextSec());
        Gdiplus::SolidBrush accentBr(Theme::GdipAccent());
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::SolidBrush cardBr(Theme::GdipBgCard());
        FillRoundRect(g, cardBr, x, y, w, h - 8, Theme::SmallRadius);

        int cy = y + 30;
        Gdiplus::RectF tRc((float)x, (float)cy, (float)w, 24);
        g.DrawString(L"Vexel V5", -1, &titleFont, tRc, &sf, &accentBr);
        cy += 34;

        const wchar_t* lines[] = {
            L"Created for educational purposes only.",
            L"For developers who know what they are doing.",
            L"",
            L"Use responsibly. The authors take no",
            L"responsibility for misuse of this software.",
            L"",
            L"Version 5.0.0",
        };
        for (auto* line : lines) {
            Gdiplus::RectF lRc((float)x, (float)cy, (float)w, 18);
            g.DrawString(line, -1, &bodyFont, lRc, &sf, &secBr);
            cy += 20;
        }
    }

    // ==================== INPUT HANDLING ====================

    void OnMouseMove(int mx, int my) {
        mousePos = { mx, my };

        // Slider dragging
        if (draggingSlider >= 0 && mouseDown) {
            RECT& sr = sliderRects[draggingSlider];
            float pct = (float)(mx - sr.left) / (float)(sr.right - sr.left);
            if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            float cps = 1.0f + pct * 49.0f;
            if (draggingSlider == 0) cfg.left.cps = cps;
            else cfg.right.cps = cps;
        }

        // Window dragging
        if (dragging) {
            POINT cursor;
            GetCursorPos(&cursor);
            RECT wr;
            GetWindowRect(hwnd, &wr);
            int dx = cursor.x - dragStart.x;
            int dy = cursor.y - dragStart.y;
            SetWindowPos(hwnd, nullptr, wr.left + dx, wr.top + dy, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
            dragStart = cursor;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void OnLButtonDown(int mx, int my) {
        mouseDown = true;
        mousePos = { mx, my };

        if (screen == Screen::Main) {
            // Check sliders (combat tab)
            if (activeTab == 1 && processConfirmed) {
                for (int i = 0; i < 2; i++) {
                    RECT& sr = sliderRects[i];
                    if (mx >= sr.left && mx <= sr.right && my >= sr.top && my <= sr.bottom) {
                        draggingSlider = i;
                        float pct = (float)(mx - sr.left) / (float)(sr.right - sr.left);
                        if (pct < 0) pct = 0; if (pct > 1) pct = 1;
                        float cps = 1.0f + pct * 49.0f;
                        if (i == 0) cfg.left.cps = cps;
                        else cfg.right.cps = cps;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return;
                    }
                }
            }

            // Title bar dragging (y < 36)
            if (my < 36) {
                // Close button
                RECT cr;
                GetClientRect(hwnd, &cr);
                int closeX = cr.right - 34;
                if (mx >= closeX && mx <= closeX + 20 && my >= 8 && my <= 28) {
                    PostQuitMessage(0);
                    return;
                }
                dragging = true;
                GetCursorPos(&dragStart);
                return;
            }

            // Tab clicks
            if (my >= 36 && my <= 36 + Theme::TabBarH) {
                for (int i = 0; i < 3; i++) {
                    int tx = 14 + i * 106;
                    if (mx >= tx && mx <= tx + 100) {
                        activeTab = i;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return;
                    }
                }
            }

            // Config tab interactions
            if (activeTab == 0) {
                HandleConfigClick(mx, my);
            }

            // Combat tab interactions
            if (activeTab == 1 && processConfirmed) {
                HandleCombatClick(mx, my);
            }
        }

        // Username screen
        if (screen == Screen::Username) {
            RECT cr;
            GetClientRect(hwnd, &cr);
            int w = cr.right;
            // Continue button
            int btnW = 140, btnH = 36;
            int btnX = (w - btnW) / 2, btnY = 165;
            if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
                if (!inputText.empty()) {
                    TransitionToMain();
                }
            }
        }

        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void OnLButtonUp(int mx, int my) {
        mouseDown = false;
        dragging = false;
        draggingSlider = -1;
    }

    void HandleConfigClick(int mx, int my) {
        RECT cr;
        GetClientRect(hwnd, &cr);
        int w = cr.right - 20; // content width
        int x = 10;
        int contentY = 36 + Theme::TabBarH + 8;

        // Refresh button
        int refX = x + w - 80;
        if (mx >= refX && mx <= refX + 80 && my >= contentY && my <= contentY + 24) {
            processes = EnumVisibleWindows();
            selectedProcess = -1;
            processConfirmed = false;
            return;
        }

        // Process list items
        int listY = contentY + 34;
        int listH = cr.bottom - contentY - 8 - 80;
        int itemH = 36;
        for (int i = 0; i < (int)processes.size(); i++) {
            int iy = listY + 6 + i * itemH - processScroll;
            if (mx >= x && mx <= x + w + 20 && my >= iy && my <= iy + itemH) {
                selectedProcess = i;
                return;
            }
        }

        // Confirm button
        int btnW2 = 160, btnH2 = 32;
        int btnX2 = x + ((w + 20) - btnW2) / 2;
        int btnY2 = listY + listH + 12;
        if (mx >= btnX2 && mx <= btnX2 + btnW2 && my >= btnY2 && my <= btnY2 + btnH2) {
            if (selectedProcess >= 0 && selectedProcess < (int)processes.size()) {
                cfg.targetPID = processes[selectedProcess].pid;
                cfg.targetName = processes[selectedProcess].windowTitle;
                processConfirmed = true;
            }
        }
    }

    void HandleCombatClick(int mx, int my) {
        RECT cr;
        GetClientRect(hwnd, &cr);
        int w = cr.right - 20;
        int halfW = (w - 12) / 2;
        int contentY = 36 + Theme::TabBarH + 8;
        int x = 10;

        // Determine which section was clicked (left or right panel)
        bool inLeft = mx >= x && mx <= x + halfW;
        bool inRight = mx >= x + halfW + 12 && mx <= x + w;
        if (!inLeft && !inRight) return;

        ClickSection& sec = inLeft ? cfg.left : cfg.right;
        int sx = inLeft ? x : x + halfW + 12;
        int pad = 14;
        int cy = contentY + pad + 28; // after title

        // CPS input box click (right side of section)
        // We don't handle inline editing for now - slider is primary

        cy += 26; // past CPS label
        cy += 36; // past slider

        // Mode buttons
        cy += 20; // past "Mode:" label
        for (int m = 0; m < 2; m++) {
            int mmx = sx + pad + m * ((halfW - pad * 2) / 2);
            int mw = (halfW - pad * 2) / 2 - 4;
            if (mx >= mmx && mx <= mmx + mw && my >= cy && my <= cy + 26) {
                sec.mode = m;
                return;
            }
        }
        cy += 34;

        // Jitter toggle
        int jw = halfW - pad * 2;
        if (mx >= sx + pad && mx <= sx + pad + jw && my >= cy && my <= cy + 26) {
            sec.jitter = !sec.jitter;
            return;
        }
        cy += 34;

        // Bind button
        int bx = sx + pad + 50;
        int bw = halfW - pad * 2 - 50;
        if (mx >= bx && mx <= bx + bw && my >= cy && my <= cy + 28) {
            sec.waitingBind = true;
            return;
        }
    }

    void OnKeyDown(int vk) {
        if (screen == Screen::Username) {
            if (vk == VK_RETURN && !inputText.empty()) {
                TransitionToMain();
            } else if (vk == VK_BACK && !inputText.empty()) {
                inputText.pop_back();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        if (screen == Screen::Main) {
            // Check bind waiting
            if (cfg.left.waitingBind) {
                cfg.left.bindKey = vk;
                cfg.left.waitingBind = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (cfg.right.waitingBind) {
                cfg.right.bindKey = vk;
                cfg.right.waitingBind = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }

            // Bind activation/deactivation
            if (cfg.left.bindKey && vk == cfg.left.bindKey) {
                ToggleClicker(cfg.left, leftClicker, true);
            }
            if (cfg.right.bindKey && vk == cfg.right.bindKey) {
                ToggleClicker(cfg.right, rightClicker, false);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    void OnChar(wchar_t ch) {
        if (screen == Screen::Username) {
            if (ch >= 32 && ch != 127 && inputText.size() < 24) {
                inputText += ch;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

    void ToggleClicker(ClickSection& sec, Clicker& clicker, bool isLeft) {
        if (sec.active) {
            sec.active = false;
            clicker.Stop();
        } else {
            sec.active = true;
            clicker.Start(sec, isLeft, cfg.targetPID);
        }
    }

    void OnTimer(UINT_PTR id) {
        if (id == (UINT_PTR)loadTimerId && screen == Screen::Loading) {
            loadProgress += 0.015f;
            if (loadProgress >= 1.0f) {
                loadProgress = 1.0f;
                TransitionToUsername();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    void OnMouseWheel(int delta) {
        if (screen == Screen::Main && activeTab == 0) {
            processScroll -= delta / 4;
            if (processScroll < 0) processScroll = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    // ==================== WndProc ====================
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (!g_gui) return DefWindowProc(hwnd, msg, wp, lp);

        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_gui->OnPaint(hdc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE:
            g_gui->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            g_gui->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            g_gui->OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_KEYDOWN:
            g_gui->OnKeyDown((int)wp);
            return 0;
        case WM_CHAR:
            g_gui->OnChar((wchar_t)wp);
            return 0;
        case WM_TIMER:
            g_gui->OnTimer(wp);
            return 0;
        case WM_MOUSEWHEEL:
            g_gui->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }
};
