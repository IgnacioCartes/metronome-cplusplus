#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <atomic>
#include <string>
#include <functional>

namespace Audio {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    void Start();
    void Stop();
    void TogglePlayPause();
    bool IsPlaying() const { return m_isPlaying.load(std::memory_order_relaxed); }

    void SetBpm(int bpm);
    int GetBpm() const { return m_bpm.load(std::memory_order_relaxed); }

    bool LoadEmbeddedSample();
    bool LoadCustomSample(const wchar_t* filePath);
    bool IsCustomSample() const { return m_isCustomSample; }
    const std::wstring& GetCustomSamplePath() const { return m_customSamplePath; }

    // Returns true if a beat triggered since last call (for UI animation)
    bool CheckAndClearBeatTrigger();

private:
    static DWORD WINAPI AudioThreadProc(LPVOID lpParam);
    void RenderLoop();

    HINSTANCE m_hInstance = nullptr;
    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
    HANDLE m_hAudioEvent = nullptr;

    IMMDeviceEnumerator* m_pEnumerator = nullptr;
    IMMDevice* m_pDevice = nullptr;
    IAudioClient* m_pAudioClient = nullptr;
    IAudioRenderClient* m_pRenderClient = nullptr;

    WAVEFORMATEX* m_pwfx = nullptr;
    uint32_t m_sampleRate = 48000;
    uint16_t m_channels = 2;
    uint16_t m_bitsPerSample = 32;
    bool m_isFloat = true;

    std::atomic<bool> m_isPlaying{false};
    std::atomic<int> m_bpm{120};
    std::atomic<uint64_t> m_framesPerBeat{24000};
    std::atomic<bool> m_beatTriggered{false};

    CRITICAL_SECTION m_sampleLock;
    std::vector<float> m_activeClickSamples;
    std::vector<float> m_embeddedClickSamples;
    bool m_isCustomSample = false;
    std::wstring m_customSamplePath;

    uint64_t m_sampleCounter = 0;
    size_t m_clickPlaybackIndex = 0;
};

} // namespace Audio
