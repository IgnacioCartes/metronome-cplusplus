#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>

namespace Audio {

// Loads and resamples a WAV sound into interleaved 32-bit float stereo samples at targetSampleRate.
// The output samples are normalized between -1.0f and 1.0f.
bool LoadWavFromMemory(const uint8_t* pData, size_t dataSize, uint32_t targetSampleRate, std::vector<float>& outStereoSamples);

bool LoadWavFromFile(const wchar_t* filePath, uint32_t targetSampleRate, std::vector<float>& outStereoSamples);

bool LoadWavFromResource(HINSTANCE hInstance, int resourceId, uint32_t targetSampleRate, std::vector<float>& outStereoSamples);

} // namespace Audio
