#include <windows.h>
#include "audio_engine.h"
#include "window.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // Enable high DPI awareness if supported
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDPIAwareFunc)();
        SetProcessDPIAwareFunc setDpiAware = reinterpret_cast<SetProcessDPIAwareFunc>(
            GetProcAddress(hUser32, "SetProcessDPIAware"));
        if (setDpiAware) {
            setDpiAware();
        }
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hr);

    Audio::AudioEngine audioEngine;
    if (!audioEngine.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize the WASAPI Audio Engine.", L"Initialization Error", MB_OK | MB_ICONERROR);
        if (coInitialized) CoUninitialize();
        return 1;
    }

    UI::MainWindow mainWindow(hInstance, audioEngine);
    if (!mainWindow.Create()) {
        MessageBoxW(nullptr, L"Failed to create the Metronome window.", L"Window Creation Error", MB_OK | MB_ICONERROR);
        audioEngine.Shutdown();
        if (coInitialized) CoUninitialize();
        return 1;
    }

    mainWindow.Show(nCmdShow);

    // Minimize working set memory footprint
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    audioEngine.Shutdown();
    if (coInitialized) {
        CoUninitialize();
    }

    return static_cast<int>(msg.wParam);
}

// Fallback WinMain for toolchains expecting ANSI WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    UNREFERENCED_PARAMETER(lpCmdLine);
    return wWinMain(hInstance, hPrevInstance, nullptr, nShowCmd);
}
