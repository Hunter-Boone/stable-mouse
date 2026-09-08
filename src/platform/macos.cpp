// SPDX-License-Identifier: GPL-3.0-only
#include "backend.h"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <QElapsedTimer>
#include <QTimer>

namespace {
constexpr int64_t tag = 0x53544D53;
class MacBackend final : public Backend {
public:
    MacBackend() {
        timer.setInterval(8); timer.setTimerType(Qt::PreciseTimer);
        connect(&timer, &QTimer::timeout, this, [this] { tick(); });
    }
    ~MacBackend() override { stop(); }
    QString instructions() const override { return "Allow Stable Mouse in System Settings > Privacy & Security > Accessibility, then enable stabilization. Relative mice and trackpads are supported; tablets and raw-input games need separate testing."; }
    QString escapeHint() const override { return "Press Control + Option + F8 anywhere to pause."; }
    void configure(FilterConfig c) override { filter.configure(c); }
    bool start(const QString &) override {
        if (active()) return true;
        const void *keys[] = {kAXTrustedCheckOptionPrompt}; const void *values[] = {kCFBooleanTrue};
        auto options = CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        const bool trusted = AXIsProcessTrustedWithOptions(options); CFRelease(options);
        if (!trusted) { emit problem("Allow Accessibility access in System Settings, then try again."); return false; }
        CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDragged) |
            CGEventMaskBit(kCGEventRightMouseDragged) | CGEventMaskBit(kCGEventOtherMouseDragged) |
            CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventOtherMouseDown);
        tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask, callback, this);
        if (!tap) { emit problem("macOS could not install the mouse filter. Check Accessibility permission and restart Stable Mouse."); return false; }
        source = CFMachPortCreateRunLoopSource(nullptr, tap, 0);
        if (!source) { stop(); emit problem("macOS could not create the input run loop."); return false; }
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        EventTypeSpec eventType{kEventClassKeyboard, kEventHotKeyPressed};
        const auto installed = InstallApplicationEventHandler(hotkeyCallback, 1, &eventType, this, &handler);
        const auto registered = RegisterEventHotKey(100, controlKey | optionKey, EventHotKeyID{0x53544D53, 1}, GetApplicationEventTarget(), 0, &hotkey);
        if (installed != noErr || registered != noErr) { stop(); emit problem("The emergency shortcut Control + Option + F8 is unavailable."); return false; }
        filter.reset(); elapsed.start(); timer.start(); CGEventTapEnable(tap, true); setActive(true); return true;
    }
    void stop() override {
        timer.stop();
        if (tap) CGEventTapEnable(tap, false);
        if (source) { CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes); CFRelease(source); source = nullptr; }
        if (tap) { CFRelease(tap); tap = nullptr; }
        if (hotkey) { UnregisterEventHotKey(hotkey); hotkey = nullptr; }
        if (handler) { RemoveEventHandler(handler); handler = nullptr; }
        filter.reset(); setActive(false);
    }
private:
    static OSStatus hotkeyCallback(EventHandlerCallRef, EventRef, void *context) {
        auto *self = static_cast<MacBackend *>(context);
        QTimer::singleShot(0, self, [self] { self->stop(); emit self->emergencyPause(); }); return noErr;
    }
    static CGEventRef callback(CGEventTapProxy, CGEventType type, CGEventRef event, void *context) {
        auto *self = static_cast<MacBackend *>(context);
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            self->timer.stop();
            QTimer::singleShot(0, self, [self] { self->stop(); emit self->problem("macOS disabled the input filter. Stabilization is paused."); });
            return event;
        }
        if (!event || CGEventGetIntegerValueField(event, kCGEventSourceUserData) == tag) return event;
        if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged || type == kCGEventRightMouseDragged || type == kCGEventOtherMouseDragged) {
            self->filter.add(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX), CGEventGetIntegerValueField(event, kCGMouseEventDeltaY));
            return nullptr;
        }
        self->filter.reset(); return event;
    }
    void tick() {
        auto delta = filter.pixels(elapsed.nsecsElapsed() / 1e9); elapsed.restart();
        if (!delta.x && !delta.y) return;
        auto current = CGEventCreate(nullptr);
        if (!current) return;
        auto p = CGEventGetLocation(current); CFRelease(current);
        p.x += delta.x; p.y += delta.y;
        // Clamp into an actual display, including layouts with gaps.
        CGDirectDisplayID displays[32]; uint32_t count = 0;
        if (CGGetActiveDisplayList(32, displays, &count) == kCGErrorSuccess && count) {
            CGPoint nearest = p; double distance = INFINITY;
            for (uint32_t i = 0; i < count; ++i) {
                auto b = CGDisplayBounds(displays[i]);
                CGPoint candidate{std::clamp(p.x, b.origin.x, b.origin.x + b.size.width - 1), std::clamp(p.y, b.origin.y, b.origin.y + b.size.height - 1)};
                double d = std::hypot(p.x - candidate.x, p.y - candidate.y);
                if (d < distance) { distance = d; nearest = candidate; }
            }
            p = nearest;
        }
        CGMouseButton button = kCGMouseButtonLeft;
        CGEventType type = kCGEventMouseMoved;
        if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft)) type = kCGEventLeftMouseDragged;
        else if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonRight)) { type = kCGEventRightMouseDragged; button = kCGMouseButtonRight; }
        else if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonCenter)) { type = kCGEventOtherMouseDragged; button = kCGMouseButtonCenter; }
        auto event = CGEventCreateMouseEvent(nullptr, type, p, button);
        if (!event) { stop(); emit problem("macOS could not create pointer events. Stabilization is paused."); return; }
        CGEventSetIntegerValueField(event, kCGEventSourceUserData, tag);
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, int64_t(delta.x));
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, int64_t(delta.y));
        CGEventPost(kCGHIDEventTap, event); CFRelease(event);
    }
    Stabilizer filter;
    QTimer timer; QElapsedTimer elapsed;
    CFMachPortRef tap = nullptr; CFRunLoopSourceRef source = nullptr;
    EventHotKeyRef hotkey = nullptr; EventHandlerRef handler = nullptr;
};
}
std::unique_ptr<Backend> createBackend() { return std::make_unique<MacBackend>(); }
