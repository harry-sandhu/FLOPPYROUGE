#include "audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <new>
#include <vector>

namespace {
    constexpr int SAMPLE_RATE = 22050;

    struct Voice {
        HWAVEOUT device = nullptr;
        WAVEHDR header = {};
        std::vector<int16_t> samples;
    };

    void CALLBACK WaveOutCallback(HWAVEOUT device, UINT message, DWORD_PTR instance,
                                  DWORD_PTR, DWORD_PTR) {
        if (message != WOM_DONE) return;
        Voice* voice = reinterpret_cast<Voice*>(instance);
        waveOutUnprepareHeader(device, &voice->header, sizeof(voice->header));
        waveOutClose(device);
        delete voice;
    }

    void PlayTone(float frequency, int durationMs, float volume, bool noisy, float slide) {
        if (durationMs <= 0 || volume <= 0.0f) return;

        Voice* voice = new (std::nothrow) Voice();
        if (!voice) return;
        int sampleCount = (SAMPLE_RATE * durationMs) / 1000;
        voice->samples.resize(static_cast<size_t>(sampleCount));

        uint32_t noiseState = 0xA341316Cu;
        for (int i = 0; i < sampleCount; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
            float progress = sampleCount > 1 ? static_cast<float>(i) / (sampleCount - 1) : 1.0f;
            float currentFrequency = frequency + slide * progress;
            float sample = std::sin(6.2831853f * currentFrequency * t);
            if (noisy) {
                noiseState = noiseState * 1664525u + 1013904223u;
                float noise = (static_cast<float>((noiseState >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
                sample = sample * 0.35f + noise * 0.65f;
            }

            float attack = std::min(1.0f, progress * 18.0f);
            float release = std::min(1.0f, (1.0f - progress) * 12.0f);
            float envelope = attack * release;
            voice->samples[static_cast<size_t>(i)] = static_cast<int16_t>(sample * volume * envelope * 32767.0f);
        }

        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = SAMPLE_RATE;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 2;
        format.nAvgBytesPerSec = SAMPLE_RATE * format.nBlockAlign;

        MMRESULT result = waveOutOpen(&voice->device, WAVE_MAPPER, &format,
                                      reinterpret_cast<DWORD_PTR>(WaveOutCallback),
                                      reinterpret_cast<DWORD_PTR>(voice), CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            delete voice;
            return;
        }

        voice->header.lpData = reinterpret_cast<LPSTR>(voice->samples.data());
        voice->header.dwBufferLength = static_cast<DWORD>(voice->samples.size() * sizeof(int16_t));
        if (waveOutPrepareHeader(voice->device, &voice->header, sizeof(voice->header)) != MMSYSERR_NOERROR ||
            waveOutWrite(voice->device, &voice->header, sizeof(voice->header)) != MMSYSERR_NOERROR) {
            waveOutReset(voice->device);
            waveOutClose(voice->device);
            delete voice;
        }
    }
}

namespace Audio {

bool Init() {
    return true;
}

void Play(Cue cue) {
    switch (cue) {
        case Cue::SHOT:          PlayTone(680.0f, 55, 0.20f, false, -120.0f); break;
        case Cue::SPECIAL_SHOT:  PlayTone(420.0f, 100, 0.24f, false, 360.0f); break;
        case Cue::HIT:           PlayTone(180.0f, 65, 0.22f, true, -70.0f); break;
        case Cue::DASH:          PlayTone(260.0f, 130, 0.22f, true, 520.0f); break;
        case Cue::PLAYER_HURT:   PlayTone(115.0f, 150, 0.28f, true, -45.0f); break;
        case Cue::PICKUP:        PlayTone(760.0f, 80, 0.20f, false, 220.0f); break;
        case Cue::CHEST:         PlayTone(310.0f, 180, 0.24f, false, 560.0f); break;
        case Cue::BOMB_PLACE:    PlayTone(150.0f, 100, 0.20f, false, -30.0f); break;
        case Cue::BOMB_EXPLODE:  PlayTone(75.0f, 260, 0.35f, true, -25.0f); break;
        case Cue::ROOM_CLEAR:    PlayTone(520.0f, 180, 0.22f, false, 420.0f); break;
        case Cue::TELEPORT:      PlayTone(900.0f, 170, 0.20f, false, -500.0f); break;
    }
}

void Shutdown() {}

} // namespace Audio
