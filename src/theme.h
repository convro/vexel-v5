#pragma once
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>

namespace Theme {
    // === Core palette (sea-green + black) ===
    inline constexpr COLORREF BgDark       = RGB(18, 18, 22);
    inline constexpr COLORREF BgPanel      = RGB(26, 28, 34);
    inline constexpr COLORREF BgCard       = RGB(34, 38, 46);
    inline constexpr COLORREF BgInput      = RGB(40, 44, 54);

    inline constexpr COLORREF Accent       = RGB(0, 200, 160);   // sea-green
    inline constexpr COLORREF AccentDim    = RGB(0, 150, 120);
    inline constexpr COLORREF AccentGlow   = RGB(0, 230, 180);

    inline constexpr COLORREF TextPrimary  = RGB(230, 235, 240);
    inline constexpr COLORREF TextSecondary= RGB(140, 150, 165);
    inline constexpr COLORREF TextMuted    = RGB(80, 90, 105);

    inline constexpr COLORREF Danger       = RGB(220, 60, 60);
    inline constexpr COLORREF Success      = RGB(0, 200, 100);
    inline constexpr COLORREF Border       = RGB(50, 55, 65);

    // GDI+ color helpers
    inline Gdiplus::Color GdipBgDark()      { return Gdiplus::Color(255, 18, 18, 22); }
    inline Gdiplus::Color GdipBgPanel()     { return Gdiplus::Color(255, 26, 28, 34); }
    inline Gdiplus::Color GdipBgCard()      { return Gdiplus::Color(255, 34, 38, 46); }
    inline Gdiplus::Color GdipBgInput()     { return Gdiplus::Color(255, 40, 44, 54); }
    inline Gdiplus::Color GdipAccent()      { return Gdiplus::Color(255, 0, 200, 160); }
    inline Gdiplus::Color GdipAccentDim()   { return Gdiplus::Color(255, 0, 150, 120); }
    inline Gdiplus::Color GdipAccentGlow()  { return Gdiplus::Color(255, 0, 230, 180); }
    inline Gdiplus::Color GdipText()        { return Gdiplus::Color(255, 230, 235, 240); }
    inline Gdiplus::Color GdipTextSec()     { return Gdiplus::Color(255, 140, 150, 165); }
    inline Gdiplus::Color GdipTextMuted()   { return Gdiplus::Color(255, 80, 90, 105); }
    inline Gdiplus::Color GdipBorder()      { return Gdiplus::Color(255, 50, 55, 65); }
    inline Gdiplus::Color GdipDanger()      { return Gdiplus::Color(255, 220, 60, 60); }

    // Layout
    inline constexpr int WinW = 700;
    inline constexpr int WinH = 580;
    inline constexpr int LoadWinW = 420;
    inline constexpr int LoadWinH = 220;
    inline constexpr int TabBarH = 42;
    inline constexpr int Radius = 10;
    inline constexpr int SmallRadius = 6;

    // Fonts (created once)
    inline const wchar_t* FontName = L"Segoe UI";
}
