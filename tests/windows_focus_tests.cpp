// SPDX-License-Identifier: GPL-3.0-only
// Controlled feasibility test only; never linked into the application.
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

int main() {
    if (!GetEnvironmentVariableW(L"STABLE_MOUSE_TEST_NATIVE_INPUT",nullptr,0)) return 77;
    HWND window=CreateWindowExW(0,L"STATIC",L"Stable Mouse focus feasibility test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                100,100,500,250,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 1;
    HWND a=CreateWindowExW(0,L"BUTTON",L"Previously focused",WS_CHILD|WS_VISIBLE|WS_TABSTOP,20,60,160,40,window,nullptr,nullptr,nullptr);
    HWND b=CreateWindowExW(0,L"BUTTON",L"Next target",WS_CHILD|WS_VISIBLE|WS_TABSTOP,220,60,160,40,window,nullptr,nullptr,nullptr);
    SetForegroundWindow(window);SetFocus(a);
    if(GetForegroundWindow()!=window) {DestroyWindow(window);std::cout<<"No interactive foreground desktop.\n";return 77;}
    RECT bounds{};GetWindowRect(b,&bounds);
    const POINT point{(bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2};
    // UI Automation queries go on a COM worker while the owning UI pumps messages.
    auto result=std::async(std::launch::async,[=] {
        if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return false;
        IUIAutomation *automation=nullptr;
        IUIAutomationElement *focused=nullptr,*underPoint=nullptr;
        bool ok=false;
        if(SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation))) &&
           SUCCEEDED(automation->GetFocusedElement(&focused)) && focused &&
           SUCCEEDED(automation->ElementFromPoint(point,&underPoint)) && underPoint) {
            UIA_HWND focusedHandle{},pointHandle{};
            ok=SUCCEEDED(focused->get_CurrentNativeWindowHandle(&focusedHandle)) &&
               SUCCEEDED(underPoint->get_CurrentNativeWindowHandle(&pointHandle)) &&
               reinterpret_cast<HWND>(focusedHandle)==a && reinterpret_cast<HWND>(pointHandle)==b;
        }
        if(underPoint)underPoint->Release();if(focused)focused->Release();if(automation)automation->Release();
        CoUninitialize();return ok;
    });
    while(result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) {
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    bool ok=result.get();DestroyWindow(window);
    std::cout<<(ok ? "PASS" : "FAIL")<<": focused control A differs from the control at candidate point B. No cursor movement or clicking was requested.\n";
    return ok ? 0 : 1;
}
