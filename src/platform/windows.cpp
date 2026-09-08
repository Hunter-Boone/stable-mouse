// SPDX-License-Identifier: GPL-3.0-only
#include "backend.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {
constexpr ULONG_PTR tag = 0x53544D53;
class WindowsBackend final : public Backend {
public:
    ~WindowsBackend() override { stop(); }
    QString instructions() const override {
        return "Works with the desktop pointer, including dragging. Games that read raw mouse input may bypass stabilization. Windows protects administrator windows from input sent by ordinary apps.";
    }
    QString escapeHint() const override { return "Press Ctrl + Alt + F8 anywhere to pause."; }
    void configure(FilterConfig c) override { strength.store(c.strength); speed.store(c.speed); }
    bool start(const QString &) override {
        if (active()) return true;
        stop();
        std::promise<QString> ready;
        auto result = ready.get_future();
        worker = std::thread([this, &ready] {
            current = this;
            MSG msg{};
            PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            threadId.store(GetCurrentThreadId());
            if (!RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F8)) {
                ready.set_value("Ctrl + Alt + F8 is already in use. Close the app using that shortcut and try again."); return;
            }
            hook = SetWindowsHookExW(WH_MOUSE_LL, callback, GetModuleHandleW(nullptr), 0);
            const UINT_PTR timer = SetTimer(nullptr, 0, 10, nullptr);
            if (!hook || !timer) {
                if (hook) UnhookWindowsHookEx(hook);
                if (timer) KillTimer(nullptr, timer);
                hook = nullptr; UnregisterHotKey(nullptr, 1);
                ready.set_value("Windows could not start the mouse filter."); return;
            }
            filter.reset();
            auto last = std::chrono::steady_clock::now();
            ready.set_value({});
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                if (msg.message == WM_HOTKEY) {
                    QMetaObject::invokeMethod(this, [this] { stop(); emit emergencyPause(); }, Qt::QueuedConnection);
                    break;
                }
                if (msg.message == WM_TIMER) {
                    const auto now = std::chrono::steady_clock::now();
                    filter.configure({strength.load(), speed.load()});
                    auto delta = filter.pixels(std::chrono::duration<double>(now - last).count()); last = now;
                    if (delta.x || delta.y) {
                        POINT p{};
                        if (!GetCursorPos(&p)) { filter.reset(); continue; }
                        const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
                        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                        INPUT input{}; input.type = INPUT_MOUSE;
                        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
                        input.mi.dx = static_cast<LONG>(std::llround(std::clamp(p.x + delta.x - left, 0.0, double(width - 1)) * 65535.0 / std::max(1, width - 1)));
                        input.mi.dy = static_cast<LONG>(std::llround(std::clamp(p.y + delta.y - top, 0.0, double(height - 1)) * 65535.0 / std::max(1, height - 1)));
                        input.mi.dwExtraInfo = tag;
                        if (SendInput(1, &input, sizeof(input)) != 1) {
                            QMetaObject::invokeMethod(this, [this] { stop(); emit problem("Windows blocked filtered input. Stabilization has paused so your mouse remains usable."); }, Qt::QueuedConnection);
                            break;
                        }
                    }
                }
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            UnhookWindowsHookEx(hook); hook = nullptr;
            KillTimer(nullptr, timer); UnregisterHotKey(nullptr, 1);
            current = nullptr;
        });
        const auto error = result.get();
        if (!error.isEmpty()) { stop(); emit problem(error); return false; }
        setActive(true); return true;
    }
    void stop() override {
        if (worker.joinable()) {
            PostThreadMessageW(threadId.load(), WM_QUIT, 0, 0);
            worker.join();
        }
        setActive(false);
    }
private:
    static LRESULT CALLBACK callback(int code, WPARAM kind, LPARAM data) {
        auto *self = current;
        if (code < 0 || !self) return CallNextHookEx(nullptr, code, kind, data);
        auto *event = reinterpret_cast<MSLLHOOKSTRUCT *>(data);
        if (event->flags & LLMHF_INJECTED) {
            if (event->dwExtraInfo != tag) self->filter.reset();
            return CallNextHookEx(nullptr, code, kind, data);
        }
        if (kind == WM_MOUSEMOVE) {
            POINT position{};
            if (!GetCursorPos(&position)) return CallNextHookEx(nullptr, code, kind, data);
            self->filter.configure({self->strength.load(), self->speed.load()});
            self->filter.add(double(event->pt.x) - position.x, double(event->pt.y) - position.y);
            return 1;
        }
        // Drop the settling tail at a press to avoid moving off the click target.
        if (kind == WM_LBUTTONDOWN || kind == WM_RBUTTONDOWN || kind == WM_MBUTTONDOWN || kind == WM_XBUTTONDOWN)
            self->filter.reset();
        return CallNextHookEx(nullptr, code, kind, data);
    }
    static thread_local WindowsBackend *current;
    std::thread worker;
    std::atomic<DWORD> threadId{0};
    std::atomic<double> strength{55}, speed{1};
    HHOOK hook = nullptr;
    Stabilizer filter;
};
thread_local WindowsBackend *WindowsBackend::current = nullptr;
}
std::unique_ptr<Backend> createBackend() { return std::make_unique<WindowsBackend>(); }
