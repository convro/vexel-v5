#pragma once
#include <Windows.h>
#include <thread>
#include <atomic>
#include <random>
#include <cmath>
#include <algorithm>
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

            // Pro mode: only click while the physical mouse button is held.
            // Use GetKeyState on the message-queue level; our synthetic
            // up/down from DoReclick won't fool it because it checks the
            // hardware-reported state at the time of the last input msg.
            if (sec.mode == 1) {
                int vkBtn = isLeft ? VK_LBUTTON : VK_RBUTTON;
                bool physDown = (GetAsyncKeyState(vkBtn) & 0x8000) != 0;
                if (!physDown) {
                    Sleep(10);
                    continue;
                }
            }

            // Click
            if (sec.mode == 1) {
                // Pro mode: release-repress to simulate clicks while
                // the physical button stays held. A full down+up would
                // cancel the physical hold and confuse GetAsyncKeyState.
                DoReclick(isLeft);
            } else {
                DoClick(isLeft);
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
                int chunk = (std::min)(10, delayMs - slept);
                Sleep(chunk);
                slept += chunk;
            }
        }
        m_running = false;
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

    // Pro mode: up then down - simulates a re-click while user
    // physically holds the button. The game registers a new click
    // but the physical hold is preserved for GetAsyncKeyState.
    void DoReclick(bool isLeft) {
        INPUT up{};
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = isLeft ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
        SendInput(1, &up, sizeof(INPUT));

        Sleep(1); // tiny gap so the game registers the release

        INPUT down{};
        down.type = INPUT_MOUSE;
        down.mi.dwFlags = isLeft ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        SendInput(1, &down, sizeof(INPUT));
    }
};
