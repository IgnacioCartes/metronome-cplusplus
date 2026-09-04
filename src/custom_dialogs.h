#pragma once

#include <windows.h>

namespace UI {

bool ShowSetBpmDialog(HWND hParent, HINSTANCE hInstance, int currentBpm, int& outBpm);

bool ShowTapTempoDialog(HWND hParent, HINSTANCE hInstance, int currentBpm, int& outBpm);

} // namespace UI
