#pragma once
#include <Windows.h>
#include <thread>
#include <atomic>
#include <random>
#include <cmath>
#include "config.h"

class Clicker {
public:
    void Start(ClickSection& sec, bool isLeft, DWORD targetPID) {
        if (m_running.load()) return;
        m_running = true;
        m_thread = std::thread([this, &sec, isLeft, targetPID]() {
            RunLoop(sec, isLeft, targetPID);
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

    void RunLoop(ClickSection& sec, bool isLeft, DWORD targetPID) {
        std::mt19937 rng(std::random_device{}());

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

            // Pro mode: only click while mouse button is held
            if (sec.mode == 1) {
                bool btnDown = isLeft
                    ? (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
                    : (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
                if (!btnDown) {
                    Sleep(5);
                    continue;
                }
            }

            // Click
            DoClick(isLeft);

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

            // Delay based on CPS with slight randomization
            float cps = sec.cps;
            if (cps < 1.0f) cps = 1.0f;
            if (cps > 100.0f) cps = 100.0f;
            double baseDelay = 1000.0 / cps;
            std::uniform_real_distribution<double> dd(0.85, 1.15);
            int delayMs = (int)(baseDelay * dd(rng));
            if (delayMs < 5) delayMs = 5;

            // Split delay into small sleeps so we can stop quickly
            int slept = 0;
            while (slept < delayMs && m_running.load() && sec.active) {
                int chunk = min(10, delayMs - slept);
                Sleep(chunk);
                slept += chunk;
            }
        }
        m_running = false;
    }

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
};
