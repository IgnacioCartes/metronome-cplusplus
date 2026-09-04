#include "audio_engine.h"
#include "wav_loader.h"
#include "resource.h"
#include <mmreg.h>
#include <algorithm>
#include <cstring>

namespace Audio {

static const GUID GUID_SUBTYPE_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

AudioEngine::AudioEngine() {
    InitializeCriticalSection(&m_sampleLock);
}

AudioEngine::~AudioEngine() {
    Shutdown();
    DeleteCriticalSection(&m_sampleLock);
}

bool AudioEngine::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_hAudioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    if (!m_hStopEvent || !m_hAudioEvent) {
        return false;
    }

    m_hThread = CreateThread(nullptr, 0, AudioThreadProc, this, 0, nullptr);
    return (m_hThread != nullptr);
}

void AudioEngine::Shutdown() {
    if (m_hStopEvent) {
        SetEvent(m_hStopEvent);
    }

    if (m_hThread) {
        WaitForSingleObject(m_hThread, 3000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }

    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }

    if (m_hAudioEvent) {
        CloseHandle(m_hAudioEvent);
        m_hAudioEvent = nullptr;
    }
}

void AudioEngine::Start() {
    // Prime the sample counter so the click fires immediately on the first buffer
    m_sampleCounter = m_framesPerBeat.load(std::memory_order_relaxed);
    m_clickPlaybackIndex = 0;
    m_isPlaying.store(true, std::memory_order_release);
}

void AudioEngine::Stop() {
    m_isPlaying.store(false, std::memory_order_release);
}

void AudioEngine::TogglePlayPause() {
    if (IsPlaying()) {
        Stop();
    } else {
        Start();
    }
}

void AudioEngine::SetBpm(int bpm) {
    if (bpm < 20) bpm = 20;
    if (bpm > 300) bpm = 300;
    m_bpm.store(bpm, std::memory_order_relaxed);

    uint64_t frames = static_cast<uint64_t>((static_cast<double>(m_sampleRate) * 60.0) / bpm);
    m_framesPerBeat.store(frames, std::memory_order_release);
}

bool AudioEngine::LoadEmbeddedSample() {
    std::vector<float> samples;
    if (!LoadWavFromResource(m_hInstance, IDR_WAV_CLICK, m_sampleRate, samples)) {
        return false;
    }

    EnterCriticalSection(&m_sampleLock);
    m_embeddedClickSamples = samples;
    m_activeClickSamples = m_embeddedClickSamples;
    m_isCustomSample = false;
    m_customSamplePath.clear();
    m_clickPlaybackIndex = 0;
    LeaveCriticalSection(&m_sampleLock);
    return true;
}

bool AudioEngine::LoadCustomSample(const wchar_t* filePath) {
    std::vector<float> samples;
    if (!LoadWavFromFile(filePath, m_sampleRate, samples)) {
        return false;
    }

    EnterCriticalSection(&m_sampleLock);
    m_activeClickSamples = samples;
    m_isCustomSample = true;
    m_customSamplePath = filePath;
    m_clickPlaybackIndex = 0;
    LeaveCriticalSection(&m_sampleLock);
    return true;
}

bool AudioEngine::CheckAndClearBeatTrigger() {
    return m_beatTriggered.exchange(false, std::memory_order_acq_rel);
}

DWORD WINAPI AudioEngine::AudioThreadProc(LPVOID lpParam) {
    AudioEngine* self = reinterpret_cast<AudioEngine*>(lpParam);
    self->RenderLoop();
    return 0;
}

void AudioEngine::RenderLoop() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInitialized = SUCCEEDED(hr);
    REFERENCE_TIME hnsBufferDuration = 500000;
    UINT32 bufferFrameCount = 0;
    HANDLE waitHandles[2] = { m_hStopEvent, m_hAudioEvent };

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&m_pEnumerator));
    if (FAILED(hr)) goto cleanup;

    hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                             reinterpret_cast<void**>(&m_pAudioClient));
    if (FAILED(hr)) goto cleanup;

    hr = m_pAudioClient->GetMixFormat(&m_pwfx);
    if (FAILED(hr) || !m_pwfx) goto cleanup;

    m_sampleRate = m_pwfx->nSamplesPerSec;
    m_channels = m_pwfx->nChannels;
    m_bitsPerSample = m_pwfx->wBitsPerSample;

    // Detect float or PCM
    m_isFloat = false;
    if (m_pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        m_isFloat = true;
    } else if (m_pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* pExt = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(m_pwfx);
        if (IsEqualGUID(pExt->SubFormat, GUID_SUBTYPE_IEEE_FLOAT)) {
            m_isFloat = true;
        }
    }

    // Load initial embedded sample at the device's native mix sample rate
    LoadEmbeddedSample();
    SetBpm(m_bpm.load());

    // 50ms buffer duration
    hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    hnsBufferDuration, 0, m_pwfx, nullptr);
    if (FAILED(hr)) goto cleanup;

    hr = m_pAudioClient->SetEventHandle(m_hAudioEvent);
    if (FAILED(hr)) goto cleanup;

    hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient),
                                    reinterpret_cast<void**>(&m_pRenderClient));
    if (FAILED(hr)) goto cleanup;

    m_pAudioClient->GetBufferSize(&bufferFrameCount);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    m_pAudioClient->Start();

    while (true) {
        DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitRes == WAIT_OBJECT_0) {
            // Stop event signaled
            break;
        }
        if (waitRes != WAIT_OBJECT_0 + 1) {
            continue;
        }

        UINT32 padding = 0;
        hr = m_pAudioClient->GetCurrentPadding(&padding);
        if (FAILED(hr)) break;

        UINT32 framesToWrite = bufferFrameCount - padding;
        if (framesToWrite == 0) continue;

        BYTE* pBuffer = nullptr;
        hr = m_pRenderClient->GetBuffer(framesToWrite, &pBuffer);
        if (FAILED(hr) || !pBuffer) continue;

        bool playing = m_isPlaying.load(std::memory_order_relaxed);
        uint64_t framesPerBeat = m_framesPerBeat.load(std::memory_order_relaxed);

        if (!playing) {
            std::memset(pBuffer, 0, framesToWrite * m_pwfx->nBlockAlign);
            m_sampleCounter = framesPerBeat; // Reset counter for immediate first beat on resume
            m_clickPlaybackIndex = 0;
        } else {
            EnterCriticalSection(&m_sampleLock);
            size_t totalClickFrames = m_activeClickSamples.size() / 2;

            if (m_isFloat) {
                float* pDst = reinterpret_cast<float*>(pBuffer);
                for (UINT32 f = 0; f < framesToWrite; ++f) {
                    m_sampleCounter++;
                    if (m_sampleCounter >= framesPerBeat) {
                        m_sampleCounter = 0;
                        m_clickPlaybackIndex = 0;
                        m_beatTriggered.store(true, std::memory_order_release);
                    }

                    float l = 0.0f;
                    float r = 0.0f;
                    if (m_clickPlaybackIndex < totalClickFrames) {
                        l = m_activeClickSamples[m_clickPlaybackIndex * 2];
                        r = m_activeClickSamples[m_clickPlaybackIndex * 2 + 1];
                        m_clickPlaybackIndex++;
                    }

                    for (uint16_t c = 0; c < m_channels; ++c) {
                        pDst[f * m_channels + c] = (c == 0) ? l : r;
                    }
                }
            } else if (m_bitsPerSample == 16) {
                int16_t* pDst = reinterpret_cast<int16_t*>(pBuffer);
                for (UINT32 f = 0; f < framesToWrite; ++f) {
                    m_sampleCounter++;
                    if (m_sampleCounter >= framesPerBeat) {
                        m_sampleCounter = 0;
                        m_clickPlaybackIndex = 0;
                        m_beatTriggered.store(true, std::memory_order_release);
                    }

                    float l = 0.0f;
                    float r = 0.0f;
                    if (m_clickPlaybackIndex < totalClickFrames) {
                        l = m_activeClickSamples[m_clickPlaybackIndex * 2];
                        r = m_activeClickSamples[m_clickPlaybackIndex * 2 + 1];
                        m_clickPlaybackIndex++;
                    }

                    int16_t sl = static_cast<int16_t>(std::clamp(l * 32767.0f, -32768.0f, 32767.0f));
                    int16_t sr = static_cast<int16_t>(std::clamp(r * 32767.0f, -32768.0f, 32767.0f));

                    for (uint16_t c = 0; c < m_channels; ++c) {
                        pDst[f * m_channels + c] = (c == 0) ? sl : sr;
                    }
                }
            } else {
                std::memset(pBuffer, 0, framesToWrite * m_pwfx->nBlockAlign);
            }
            LeaveCriticalSection(&m_sampleLock);
        }

        m_pRenderClient->ReleaseBuffer(framesToWrite, 0);
    }

cleanup:
    if (m_pAudioClient) {
        m_pAudioClient->Stop();
    }
    if (m_pRenderClient) {
        m_pRenderClient->Release();
        m_pRenderClient = nullptr;
    }
    if (m_pAudioClient) {
        m_pAudioClient->Release();
        m_pAudioClient = nullptr;
    }
    if (m_pwfx) {
        CoTaskMemFree(m_pwfx);
        m_pwfx = nullptr;
    }
    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }
    if (coInitialized) {
        CoUninitialize();
    }
}

} // namespace Audio
