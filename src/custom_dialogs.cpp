#include "custom_dialogs.h"
#include "resource.h"
#include <vector>
#include <string>
#include <numeric>
#include <cstdint>
#include <cmath>

namespace UI {

struct SetBpmDialogData {
    int currentBpm;
    int resultBpm;
};

static INT_PTR CALLBACK SetBpmDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetBpmDialogData* pData = reinterpret_cast<SetBpmDialogData*>(lParam);
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pData));
            SetDlgItemInt(hDlg, IDC_EDIT_BPM, pData->currentBpm, FALSE);
            SendDlgItemMessageW(hDlg, IDC_EDIT_BPM, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hDlg, IDC_EDIT_BPM));
            return FALSE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDOK) {
                BOOL translated = FALSE;
                UINT bpm = GetDlgItemInt(hDlg, IDC_EDIT_BPM, &translated, FALSE);
                if (!translated || bpm < 20 || bpm > 300) {
                    MessageBoxW(hDlg, L"Please enter a valid BPM between 20 and 300.", L"Invalid Tempo", MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(hDlg, IDC_EDIT_BPM));
                    return TRUE;
                }
                SetBpmDialogData* pData = reinterpret_cast<SetBpmDialogData*>(GetWindowLongPtrW(hDlg, DWLP_USER));
                if (pData) {
                    pData->resultBpm = static_cast<int>(bpm);
                }
                EndDialog(hDlg, IDOK);
                return TRUE;
            } else if (id == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

bool ShowSetBpmDialog(HWND hParent, HINSTANCE hInstance, int currentBpm, int& outBpm) {
    SetBpmDialogData data;
    data.currentBpm = currentBpm;
    data.resultBpm = currentBpm;

    INT_PTR res = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_SET_BPM), hParent, SetBpmDlgProc, reinterpret_cast<LPARAM>(&data));
    if (res == IDOK) {
        outBpm = data.resultBpm;
        return true;
    }
    return false;
}

struct TapTempoData {
    int currentBpm;
    int calculatedBpm;
    std::vector<int64_t> tapTimesQpc;
    int64_t qpcFrequency;
};

static void UpdateTapTempoUI(HWND hDlg, TapTempoData* pData) {
    if (pData->calculatedBpm > 0) {
        wchar_t buf[64];
        wsprintfW(buf, L"Tempo: %d BPM", pData->calculatedBpm);
        SetDlgItemTextW(hDlg, IDC_STATIC_TAP_BPM, buf);
    } else {
        SetDlgItemTextW(hDlg, IDC_STATIC_TAP_BPM, L"Tempo: -- BPM");
    }

    wchar_t countBuf[64];
    wsprintfW(countBuf, L"Taps recorded: %d", static_cast<int>(pData->tapTimesQpc.size()));
    SetDlgItemTextW(hDlg, IDC_STATIC_TAP_COUNT, countBuf);
}

static void RecordTap(HWND hDlg, TapTempoData* pData) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (!pData->tapTimesQpc.empty()) {
        int64_t last = pData->tapTimesQpc.back();
        double elapsedSec = static_cast<double>(now.QuadPart - last) / static_cast<double>(pData->qpcFrequency);
        // If more than 2.5 seconds have elapsed, reset the tap stream
        if (elapsedSec > 2.5) {
            pData->tapTimesQpc.clear();
        }
    }

    pData->tapTimesQpc.push_back(now.QuadPart);
    if (pData->tapTimesQpc.size() > 16) {
        pData->tapTimesQpc.erase(pData->tapTimesQpc.begin());
    }

    if (pData->tapTimesQpc.size() >= 2) {
        double totalDiff = 0.0;
        for (size_t i = 1; i < pData->tapTimesQpc.size(); ++i) {
            totalDiff += static_cast<double>(pData->tapTimesQpc[i] - pData->tapTimesQpc[i - 1]);
        }
        double avgTicks = totalDiff / (pData->tapTimesQpc.size() - 1);
        double avgSec = avgTicks / static_cast<double>(pData->qpcFrequency);
        if (avgSec > 0.0) {
            int bpm = static_cast<int>(std::round(60.0 / avgSec));
            if (bpm < 20) bpm = 20;
            if (bpm > 300) bpm = 300;
            pData->calculatedBpm = bpm;
        }
    }

    UpdateTapTempoUI(hDlg, pData);
}

static INT_PTR CALLBACK TapTempoDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            TapTempoData* pData = reinterpret_cast<TapTempoData*>(lParam);
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pData));
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            pData->qpcFrequency = freq.QuadPart;
            pData->calculatedBpm = 0;
            pData->tapTimesQpc.clear();
            UpdateTapTempoUI(hDlg, pData);
            SetFocus(GetDlgItem(hDlg, IDC_BTN_TAP));
            return FALSE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            TapTempoData* pData = reinterpret_cast<TapTempoData*>(GetWindowLongPtrW(hDlg, DWLP_USER));
            if (!pData) return FALSE;

            if (id == IDC_BTN_TAP) {
                RecordTap(hDlg, pData);
                return TRUE;
            } else if (id == IDC_BTN_TAP_RESET) {
                pData->tapTimesQpc.clear();
                pData->calculatedBpm = 0;
                UpdateTapTempoUI(hDlg, pData);
                SetFocus(GetDlgItem(hDlg, IDC_BTN_TAP));
                return TRUE;
            } else if (id == IDC_BTN_TAP_APPLY || id == IDOK) {
                if (pData->calculatedBpm >= 20 && pData->calculatedBpm <= 300) {
                    EndDialog(hDlg, IDOK);
                } else {
                    MessageBoxW(hDlg, L"Please tap at least twice to establish a tempo.", L"Tap Tempo", MB_OK | MB_ICONINFORMATION);
                    SetFocus(GetDlgItem(hDlg, IDC_BTN_TAP));
                }
                return TRUE;
            } else if (id == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

bool ShowTapTempoDialog(HWND hParent, HINSTANCE hInstance, int currentBpm, int& outBpm) {
    TapTempoData data;
    data.currentBpm = currentBpm;
    data.calculatedBpm = currentBpm;

    INT_PTR res = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_TAP_TEMPO), hParent, TapTempoDlgProc, reinterpret_cast<LPARAM>(&data));
    if (res == IDOK && data.calculatedBpm >= 20 && data.calculatedBpm <= 300) {
        outBpm = data.calculatedBpm;
        return true;
    }
    return false;
}

} // namespace UI
