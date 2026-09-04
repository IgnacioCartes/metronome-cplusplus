#pragma once

#include <windows.h>
#include <string>
#include "audio_engine.h"

namespace UI {

class MainWindow {
public:
    MainWindow(HINSTANCE hInstance, Audio::AudioEngine& audioEngine);
    ~MainWindow();

    bool Create();
    void Show(int nCmdShow);
    HWND GetHwnd() const { return m_hWnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnPaint(HDC hdc);
    void OnContextMenu(int x, int y);
    void ToggleAlwaysOnTop();
    void BrowseCustomSample();
    void SetBpmAndRefresh(int bpm);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    Audio::AudioEngine& m_audioEngine;

    bool m_isAlwaysOnTop = true;
    bool m_isDragging = false;
    POINT m_mouseDownPos = {0, 0};

    // Animation & rendering
    HFONT m_hFontBpm = nullptr;
    HFONT m_hFontStatus = nullptr;
    HFONT m_hFontHint = nullptr;
    float m_beatPulseAlpha = 0.0f;

    static const int WINDOW_WIDTH = 220;
    static const int WINDOW_HEIGHT = 130;
};

} // namespace UI
