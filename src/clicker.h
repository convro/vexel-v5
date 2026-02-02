#pragma once
#include <Windows.h>
#include <timeapi.h>
#include <thread>
#include <atomic>
#include <random>
#include <cmath>
#include <chrono>
#include <algorithm>
#include "config.h"

#pragma comment(lib, "winmm.lib")

class Clicker {
public:
    void Start(ClickSection& sec, bool isLeft, DWORD targetPID, AppConfig* appCfg = nullptr) {
        if (m_running.load()) return;
        m_running = true;
        m_thread = std::thread([this, &sec, isLeft, targetPID, appCfg]() {
            RunLoop(sec, isLeft, targetPID, appCfg);
        });
        m_thread.detach();
    }

    void Stop() {
        m_running = false;
    }

    bool IsRunning() const { return m_running.load(); }

private:
    std::atomic<bool> m_running{ false };
    std::thread m_thread;

    void RunLoop(ClickSection& sec, bool isLeft, DWORD targetPID, AppConfig* appCfg) {
        std::mt19937 rng(std::random_device{}());

        // Request 1ms timer resolution for accurate Sleep
        timeBeginPeriod(1);

        while (m_running.load() && sec.active) {
            // Safety: only click when target window is focused
            if (targetPID != 0) {
                HWND fg = GetForegroundWindow();
                DWORD fgPid = 0;
                if (fg) GetWindowThreadProcessId(fg, &fgPid);
                if (fgPid != targetPID) {
                    Sleep(50);
                    continue;
                }
            }

            // Pro mode: only click while the physical mouse button is held
            if (sec.mode == 1) {
                int vkBtn = isLeft ? VK_LBUTTON : VK_RBUTTON;
                bool physDown = (GetAsyncKeyState(vkBtn) & 0x8000) != 0;
                if (!physDown) {
                    Sleep(1);
                    continue;
                }
            }

            // Click
            if (sec.mode == 1) {
                DoReclick(isLeft);
            } else {
                DoClick(isLeft);
            }

            // Auto Guard: during left clicking, occasionally right click
            if (isLeft && appCfg && appCfg->autoGuard) {
                std::uniform_real_distribution<float> chance(0.0f, 100.0f);
                if (chance(rng) < appCfg->autoGuardRate) {
                    DoClick(false); // right click
                }
            }

            // Jitter
            if (sec.jitter) {
                std::uniform_int_distribution<int> jd(-3, 3);
                INPUT ji{};
                ji.type = INPUT_MOUSE;
                ji.mi.dx = jd(rng);
                ji.mi.dy = jd(rng);
                ji.mi.dwFlags = MOUSEEVENTF_MOVE;
                SendInput(1, &ji, sizeof(INPUT));
            }

            // Delay based on CPS with randomizer
            float cps = sec.cps;
            if (cps < 1.0f) cps = 1.0f;
            if (cps > 50.0f) cps = 50.0f;

            double baseDelay = 1000.0 / cps;

            // Randomizer: adds variance proportional to randomizer %
            // e.g. 50% randomizer at 20 CPS = delay varies +/-25% around base
            float randPct = sec.randomizer / 100.0f; // 0.0 - 1.0
            float lo = 1.0f - randPct * 0.5f;
            float hi = 1.0f + randPct * 0.5f;
            if (lo < 0.3f) lo = 0.3f;
            std::uniform_real_distribution<double> dd((double)lo, (double)hi);
            double delayMs = baseDelay * dd(rng);
            if (delayMs < 2.0) delayMs = 2.0;

            // Precise sleep using busy-wait for the last few ms
            PreciseSleep(delayMs, sec, rng);
        }

        timeEndPeriod(1);
        m_running = false;
    }

    // Hybrid sleep: Sleep for bulk, busy-wait for remainder
    void PreciseSleep(double ms, ClickSection& sec, std::mt19937& rng) {
        auto start = std::chrono::high_resolution_clock::now();
        double targetUs = ms * 1000.0;

        // Sleep most of the time (leave 1.5ms for busy-wait)
        int sleepMs = (int)(ms - 1.5);
        if (sleepMs > 0) {
            // Split into small chunks so we can bail quickly
            int slept = 0;
            while (slept < sleepMs && m_running.load() && sec.active) {
                int chunk = (std::min)(4, sleepMs - slept);
                Sleep(chunk);
                slept += chunk;
            }
            if (!m_running.load() || !sec.active) return;
        }

        // Busy-wait the remainder for precision
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::micro>(now - start).count();
            if (elapsed >= targetUs) break;
            if (!m_running.load() || !sec.active) break;
            // Yield to avoid 100% CPU if we have a lot of time left
            if (targetUs - elapsed > 500) {
                Sleep(0);
            }
        }
    }

    // Basic mode: full click (down + up)
    void DoClick(bool isLeft) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[1].type = INPUT_MOUSE;
        if (isLeft) {
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        } else {
            inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        }
        SendInput(2, inputs, sizeof(INPUT));
    }

    // Pro mode: up then down - game registers new click, physical hold preserved
    void DoReclick(bool isLeft) {
        INPUT up{};
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = isLeft ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
        SendInput(1, &up, sizeof(INPUT));

        Sleep(1);

        INPUT down{};
        down.type = INPUT_MOUSE;
        down.mi.dwFlags = isLeft ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        SendInput(1, &down, sizeof(INPUT));
    }
};
