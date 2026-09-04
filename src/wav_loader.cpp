#include "wav_loader.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Audio {

#pragma pack(push, 1)
struct RiffHeader {
    char riffId[4];
    uint32_t fileSize;
    char waveId[4];
};

struct ChunkHeader {
    char id[4];
    uint32_t size;
};

struct WaveFmtChunk {
    uint16_t formatTag;
    uint16_t channels;
    uint32_t samplesPerSec;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

static const uint8_t GUID_PCM[16] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};
static const uint8_t GUID_FLOAT[16] = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

bool LoadWavFromMemory(const uint8_t* pData, size_t dataSize, uint32_t targetSampleRate, std::vector<float>& outStereoSamples) {
    outStereoSamples.clear();
    if (!pData || dataSize < sizeof(RiffHeader)) {
        return false;
    }

    const RiffHeader* pRiff = reinterpret_cast<const RiffHeader*>(pData);
    if (std::memcmp(pRiff->riffId, "RIFF", 4) != 0 || std::memcmp(pRiff->waveId, "WAVE", 4) != 0) {
        return false;
    }

    size_t offset = sizeof(RiffHeader);
    WaveFmtChunk fmt = {};
    bool fmtFound = false;
    bool isFloat = false;
    const uint8_t* pAudioData = nullptr;
    uint32_t audioDataSize = 0;

    while (offset + sizeof(ChunkHeader) <= dataSize) {
        const ChunkHeader* pChunk = reinterpret_cast<const ChunkHeader*>(pData + offset);
        offset += sizeof(ChunkHeader);

        uint32_t chunkSize = pChunk->size;
        if (offset + chunkSize > dataSize) {
            chunkSize = static_cast<uint32_t>(dataSize - offset);
        }

        if (std::memcmp(pChunk->id, "fmt ", 4) == 0 && chunkSize >= sizeof(WaveFmtChunk)) {
            std::memcpy(&fmt, pData + offset, sizeof(WaveFmtChunk));
            fmtFound = true;

            if (fmt.formatTag == 3) {
                isFloat = true;
            } else if (fmt.formatTag == 0xFFFE && chunkSize >= 40) { // WAVE_FORMAT_EXTENSIBLE
                const uint8_t* pSubFormat = pData + offset + 24;
                if (std::memcmp(pSubFormat, GUID_FLOAT, 16) == 0) {
                    isFloat = true;
                }
            }
        } else if (std::memcmp(pChunk->id, "data", 4) == 0) {
            pAudioData = pData + offset;
            audioDataSize = chunkSize;
            break; // found data chunk
        }

        // RIFF chunks are word-aligned (2-byte padding)
        offset += chunkSize + (chunkSize & 1);
    }

    if (!fmtFound || !pAudioData || audioDataSize == 0) {
        return false;
    }
    if (fmt.channels < 1 || fmt.channels > 2) {
        return false; // Only mono and stereo supported
    }

    // Decode source audio frames into normalized float [-1.0f, +1.0f]
    size_t bytesPerFrame = (fmt.bitsPerSample / 8) * fmt.channels;
    if (bytesPerFrame == 0) return false;

    size_t sourceFrames = audioDataSize / bytesPerFrame;
    if (sourceFrames == 0) return false;

    std::vector<float> decodedSource(sourceFrames * fmt.channels);

    for (size_t f = 0; f < sourceFrames; ++f) {
        const uint8_t* pFrame = pAudioData + f * bytesPerFrame;
        for (uint16_t c = 0; c < fmt.channels; ++c) {
            float sample = 0.0f;
            const uint8_t* pSample = pFrame + c * (fmt.bitsPerSample / 8);

            if (isFloat && fmt.bitsPerSample == 32) {
                sample = *reinterpret_cast<const float*>(pSample);
            } else if (fmt.bitsPerSample == 8) {
                uint8_t val = *pSample;
                sample = (static_cast<float>(val) - 128.0f) / 128.0f;
            } else if (fmt.bitsPerSample == 16) {
                int16_t val = *reinterpret_cast<const int16_t*>(pSample);
                sample = static_cast<float>(val) / 32768.0f;
            } else if (fmt.bitsPerSample == 24) {
                int32_t val = (static_cast<int32_t>(pSample[0])) |
                              (static_cast<int32_t>(pSample[1]) << 8) |
                              (static_cast<int32_t>(static_cast<int8_t>(pSample[2])) << 16);
                sample = static_cast<float>(val) / 8388608.0f;
            } else if (fmt.bitsPerSample == 32) {
                int32_t val = *reinterpret_cast<const int32_t*>(pSample);
                sample = static_cast<float>(val) / 2147483648.0f;
            }

            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            decodedSource[f * fmt.channels + c] = sample;
        }
    }

    // Resample to targetSampleRate if necessary and convert to stereo
    double sampleRateRatio = static_cast<double>(targetSampleRate) / static_cast<double>(fmt.samplesPerSec);
    size_t targetFrames = static_cast<size_t>(std::round(sourceFrames * sampleRateRatio));
    if (targetFrames == 0) targetFrames = 1;

    outStereoSamples.resize(targetFrames * 2);

    for (size_t t = 0; t < targetFrames; ++t) {
        double srcPos = static_cast<double>(t) / sampleRateRatio;
        size_t idx0 = static_cast<size_t>(srcPos);
        size_t idx1 = (idx0 + 1 < sourceFrames) ? (idx0 + 1) : idx0;
        float frac = static_cast<float>(srcPos - idx0);

        float leftSample = 0.0f;
        float rightSample = 0.0f;

        if (fmt.channels == 1) {
            float s0 = decodedSource[idx0];
            float s1 = decodedSource[idx1];
            float s = s0 + frac * (s1 - s0);
            leftSample = s;
            rightSample = s;
        } else { // stereo
            float l0 = decodedSource[idx0 * 2];
            float l1 = decodedSource[idx1 * 2];
            leftSample = l0 + frac * (l1 - l0);

            float r0 = decodedSource[idx0 * 2 + 1];
            float r1 = decodedSource[idx1 * 2 + 1];
            rightSample = r0 + frac * (r1 - r0);
        }

        outStereoSamples[t * 2]     = leftSample;
        outStereoSamples[t * 2 + 1] = rightSample;
    }

    return true;
}

bool LoadWavFromFile(const wchar_t* filePath, uint32_t targetSampleRate, std::vector<float>& outStereoSamples) {
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<uint8_t> buffer(fileSize);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesRead != fileSize) {
        return false;
    }

    return LoadWavFromMemory(buffer.data(), buffer.size(), targetSampleRate, outStereoSamples);
}

bool LoadWavFromResource(HINSTANCE hInstance, int resourceId, uint32_t targetSampleRate, std::vector<float>& outStereoSamples) {
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), L"WAVE");
    if (!hRes) {
        return false;
    }

    HGLOBAL hMem = LoadResource(hInstance, hRes);
    if (!hMem) {
        return false;
    }

    const uint8_t* pData = reinterpret_cast<const uint8_t*>(LockResource(hMem));
    DWORD size = SizeofResource(hInstance, hRes);
    if (!pData || size == 0) {
        return false;
    }

    return LoadWavFromMemory(pData, size, targetSampleRate, outStereoSamples);
}

} // namespace Audio
