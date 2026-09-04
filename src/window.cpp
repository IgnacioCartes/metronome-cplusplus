#include "window.h"
#include "resource.h"
#include "custom_dialogs.h"
#include <windowsx.h>
#include <commdlg.h>
#include <cmath>

namespace UI {

static const wchar_t* WINDOW_CLASS_NAME = L"LightweightMetronomeWindowClass";

MainWindow::MainWindow(HINSTANCE hInstance, Audio::AudioEngine& audioEngine)
    : m_hInstance(hInstance), m_audioEngine(audioEngine) {}

MainWindow::~MainWindow() {
    if (m_hFontBpm) DeleteObject(m_hFontBpm);
    if (m_hFontStatus) DeleteObject(m_hFontStatus);
    if (m_hFontHint) DeleteObject(m_hFontHint);
}

bool MainWindow::Create() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(MainWindow*);
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); // IDI_APPLICATION
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.hbrBackground = nullptr; // Handled in WM_PAINT
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wc);

    // Screen center coordinates
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - WINDOW_WIDTH) / 2;
    int posY = (screenH - WINDOW_HEIGHT) / 2;

    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (m_isAlwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }

    m_hWnd = CreateWindowExW(
        exStyle,
        WINDOW_CLASS_NAME,
        L"Metronome",
        WS_POPUP,
        posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, m_hInstance, this
    );

    return (m_hWnd != nullptr);
}

void MainWindow::Show(int nCmdShow) {
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            m_hFontBpm = CreateFontW(44, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            m_hFontStatus = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            m_hFontHint = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            // 60 Hz timer for beat pulse decay and UI update
            SetTimer(m_hWnd, 1, 16, nullptr);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                bool newBeat = m_audioEngine.CheckAndClearBeatTrigger();
                bool needsRepaint = false;

                if (newBeat) {
                    m_beatPulseAlpha = 1.0f;
                    needsRepaint = true;
                } else if (m_beatPulseAlpha > 0.01f) {
                    m_beatPulseAlpha *= 0.82f;
                    if (m_beatPulseAlpha < 0.02f) {
                        m_beatPulseAlpha = 0.0f;
                    }
                    needsRepaint = true;
                }

                if (needsRepaint) {
                    InvalidateRect(m_hWnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Close button in top-right corner (width-22 to width-4, 4 to 22)
            if (x >= WINDOW_WIDTH - 24 && x <= WINDOW_WIDTH - 4 && y >= 4 && y <= 24) {
                DestroyWindow(m_hWnd);
                return 0;
            }

            m_isDragging = false;
            m_mouseDownPos.x = x;
            m_mouseDownPos.y = y;
            SetCapture(m_hWnd);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (GetCapture() == m_hWnd) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int dx = std::abs(x - m_mouseDownPos.x);
                int dy = std::abs(y - m_mouseDownPos.y);

                if (dx > 3 || dy > 3) {
                    m_isDragging = true;
                    ReleaseCapture();
                    // Initiate standard Win32 window drag
                    SendMessageW(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (GetCapture() == m_hWnd) {
                ReleaseCapture();
                if (!m_isDragging) {
                    // Click without drag toggles Play/Pause
                    m_audioEngine.TogglePlayPause();
                    InvalidateRect(m_hWnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            POINT pt = { x, y };
            ClientToScreen(m_hWnd, &pt);
            OnContextMenu(pt.x, pt.y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int step = 1;
            if (wParam & MK_SHIFT) step = 5;
            if (wParam & MK_CONTROL) step = 10;

            int change = (delta > 0) ? step : -step;
            SetBpmAndRefresh(m_audioEngine.GetBpm() + change);
            return 0;
        }

        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_SPACE:
                    m_audioEngine.TogglePlayPause();
                    InvalidateRect(m_hWnd, nullptr, FALSE);
                    return 0;
                case VK_UP:
                case VK_RIGHT: {
                    int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 5 : 1;
                    SetBpmAndRefresh(m_audioEngine.GetBpm() + step);
                    return 0;
                }
                case VK_DOWN:
                case VK_LEFT: {
                    int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 5 : 1;
                    SetBpmAndRefresh(m_audioEngine.GetBpm() - step);
                    return 0;
                }
                case 'T': {
                    int newBpm = m_audioEngine.GetBpm();
                    if (ShowTapTempoDialog(m_hWnd, m_hInstance, newBpm, newBpm)) {
                        SetBpmAndRefresh(newBpm);
                    }
                    return 0;
                }
                case VK_ESCAPE:
                    DestroyWindow(m_hWnd);
                    return 0;
            }
            break;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDM_PLAY_PAUSE:
                    m_audioEngine.TogglePlayPause();
                    InvalidateRect(m_hWnd, nullptr, FALSE);
                    break;
                case IDM_BPM_60:  SetBpmAndRefresh(60); break;
                case IDM_BPM_90:  SetBpmAndRefresh(90); break;
                case IDM_BPM_120: SetBpmAndRefresh(120); break;
                case IDM_BPM_140: SetBpmAndRefresh(140); break;
                case IDM_BPM_160: SetBpmAndRefresh(160); break;
                case IDM_BPM_180: SetBpmAndRefresh(180); break;
                case IDM_BPM_200: SetBpmAndRefresh(200); break;
                case IDM_CUSTOM_BPM: {
                    int newBpm = m_audioEngine.GetBpm();
                    if (ShowSetBpmDialog(m_hWnd, m_hInstance, newBpm, newBpm)) {
                        SetBpmAndRefresh(newBpm);
                    }
                    break;
                }
                case IDM_TAP_TEMPO: {
                    int newBpm = m_audioEngine.GetBpm();
                    if (ShowTapTempoDialog(m_hWnd, m_hInstance, newBpm, newBpm)) {
                        SetBpmAndRefresh(newBpm);
                    }
                    break;
                }
                case IDM_SAMPLE_EMBEDDED:
                    m_audioEngine.LoadEmbeddedSample();
                    break;
                case IDM_SAMPLE_CUSTOM:
                    BrowseCustomSample();
                    break;
                case IDM_ALWAYS_ON_TOP:
                    ToggleAlwaysOnTop();
                    break;
                case IDM_EXIT:
                    DestroyWindow(m_hWnd);
                    break;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);
            OnPaint(hdc);
            EndPaint(m_hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker; background rendered in OnPaint

        case WM_DESTROY: {
            KillTimer(m_hWnd, 1);
            m_audioEngine.Stop();
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
}

void MainWindow::SetBpmAndRefresh(int bpm) {
    m_audioEngine.SetBpm(bpm);
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void MainWindow::ToggleAlwaysOnTop() {
    m_isAlwaysOnTop = !m_isAlwaysOnTop;
    SetWindowPos(m_hWnd, m_isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void MainWindow::BrowseCustomSample() {
    wchar_t szFile[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"WAV Audio Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        if (!m_audioEngine.LoadCustomSample(szFile)) {
            MessageBoxW(m_hWnd, L"Failed to load the selected WAV file. Ensure it is a valid PCM or Float WAV.",
                        L"Audio Error", MB_OK | MB_ICONERROR);
        }
    }
}

void MainWindow::OnContextMenu(int x, int y) {
    HMENU hMenu = LoadMenuW(m_hInstance, MAKEINTRESOURCEW(IDR_CONTEXT_MENU));
    if (!hMenu) return;

    HMENU hSubMenu = GetSubMenu(hMenu, 0);
    if (!hSubMenu) {
        DestroyMenu(hMenu);
        return;
    }

    // Check Play / Pause state
    bool isPlaying = m_audioEngine.IsPlaying();
    CheckMenuItem(hSubMenu, IDM_PLAY_PAUSE, MF_BYCOMMAND | (isPlaying ? MF_CHECKED : MF_UNCHECKED));

    // Check Always on Top state
    CheckMenuItem(hSubMenu, IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (m_isAlwaysOnTop ? MF_CHECKED : MF_UNCHECKED));

    // Check Audio Sample selection
    bool isCustom = m_audioEngine.IsCustomSample();
    CheckMenuRadioItem(hSubMenu, IDM_SAMPLE_EMBEDDED, IDM_SAMPLE_CUSTOM,
                       isCustom ? IDM_SAMPLE_CUSTOM : IDM_SAMPLE_EMBEDDED, MF_BYCOMMAND);

    // Check Preset radio item if current BPM matches
    int bpm = m_audioEngine.GetBpm();
    UINT presetId = 0;
    switch (bpm) {
        case 60:  presetId = IDM_BPM_60; break;
        case 90:  presetId = IDM_BPM_90; break;
        case 120: presetId = IDM_BPM_120; break;
        case 140: presetId = IDM_BPM_140; break;
        case 160: presetId = IDM_BPM_160; break;
        case 180: presetId = IDM_BPM_180; break;
        case 200: presetId = IDM_BPM_200; break;
    }
    if (presetId != 0) {
        CheckMenuRadioItem(hSubMenu, IDM_BPM_60, IDM_BPM_200, presetId, MF_BYCOMMAND);
    }

    SetForegroundWindow(m_hWnd);
    TrackPopupMenuEx(hSubMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, x, y, m_hWnd, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::OnPaint(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    // Double-buffered rendering to eliminate any flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Background color: Modern slate dark (#16181F)
    HBRUSH bgBrush = CreateSolidBrush(RGB(22, 24, 31));
    FillRect(memDC, &rcClient, bgBrush);
    DeleteObject(bgBrush);

    // 1px Border (#353B4B)
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(53, 59, 75));
    HGDIOBJ oldPen = SelectObject(memDC, borderPen);
    HGDIOBJ oldNullBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, 0, 0, width, height);

    // Beat Pulse Indicator Bar at top (Height 4px)
    // Lerp from idle dark gray to vibrant cyan (#00E5FF) based on m_beatPulseAlpha
    float alpha = m_beatPulseAlpha;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    int r = static_cast<int>(35 + alpha * (0 - 35));
    int g = static_cast<int>(40 + alpha * (229 - 40));
    int b = static_cast<int>(55 + alpha * (255 - 55));
    HBRUSH pulseBrush = CreateSolidBrush(RGB(r, g, b));
    RECT pulseRect = { 1, 1, width - 1, 5 };
    FillRect(memDC, &pulseRect, pulseBrush);
    DeleteObject(pulseBrush);

    // Top-right Close 'x' icon
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(120, 130, 145));
    SelectObject(memDC, m_hFontStatus);
    RECT closeRect = { width - 20, 4, width - 4, 20 };
    DrawTextW(memDC, L"\u00D7", 1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // BPM Large Display (Center)
    int bpm = m_audioEngine.GetBpm();
    wchar_t bpmText[16];
    wsprintfW(bpmText, L"%d", bpm);

    SelectObject(memDC, m_hFontBpm);
    SetTextColor(memDC, RGB(245, 247, 250));
    RECT bpmRect = { 0, 18, width, 68 };
    DrawTextW(memDC, bpmText, -1, &bpmRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Subtitle: State & Unit
    bool isPlaying = m_audioEngine.IsPlaying();
    SelectObject(memDC, m_hFontStatus);
    if (isPlaying) {
        SetTextColor(memDC, RGB(0, 230, 118)); // Vibrant Emerald Green
        RECT statusRect = { 0, 68, width, 86 };
        DrawTextW(memDC, L"BPM  \u25CF  RUNNING", -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(memDC, RGB(255, 171, 0)); // Warm Amber
        RECT statusRect = { 0, 68, width, 86 };
        DrawTextW(memDC, L"BPM  \u25CB  PAUSED", -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Footer Hint
    SelectObject(memDC, m_hFontHint);
    SetTextColor(memDC, RGB(105, 115, 132));
    RECT hintRect = { 0, 94, width, 122 };
    DrawTextW(memDC, L"Click: Play/Pause \u2022 Scroll: BPM\nRight-click: Menu \u2022 Drag: Move", -1, &hintRect, DT_CENTER | DT_VCENTER);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // Cleanup GDI objects
    SelectObject(memDC, oldNullBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace UI
