#pragma once

#include <cstddef>
#include <cstdint>

namespace ai_caption {
namespace constants {

/* ---- ASR decode thread ---- */
constexpr size_t kDecodeChunkSamples = 1600;   /* 100ms @ 16kHz, matches AISDK frame size */
constexpr int kRingWaitTimeoutMs = 50;
constexpr size_t kAsrRingCapacity = 64000;     /* 4s @ 16kHz before dropping samples */

/* ---- Caption file output ---- */
constexpr uint64_t kPartialWriteThrottleMs = 80;
constexpr uint64_t kAsrStatsLogIntervalMs = 2000;

/* ---- Audio analyzer (pitch + RMS) ---- */
constexpr uint32_t kAnalyzerWindowMs = 30;
constexpr float kRmsSmoothingAlpha = 0.15f;        /* EMA weight on new sample */
constexpr float kPitchSmoothingAlpha = 0.30f;      /* EMA weight on new pitch estimate */
constexpr float kMinPitchHz = 80.0f;
constexpr float kMaxPitchHz = 500.0f;
constexpr float kPitchCorrelationThreshold = 0.25f;
constexpr int kPitchUpdateDivisor = 12;            /* update every sample_rate / 12 ≈ 83ms */

/* ---- Beep generator (adaptive censor tone) ---- */
constexpr float kBeepFreqMultiplier = 5.0f;        /* scale detected F0 up into beep range */
constexpr float kBeepFreqMinHz = 500.0f;
constexpr float kBeepFreqMaxHz = 2000.0f;
constexpr float kBeepVolumeMultiplier = 2.0f;
constexpr float kBeepVolumeMin = 0.05f;
constexpr float kBeepVolumeMax = 0.9f;

/* ---- Mute word detection ---- */
constexpr float kDefaultAsrLatencySec = 0.6f;
constexpr float kHotwordsScore = 2.0f;
constexpr int kHotwordsMaxActivePaths = 8;
constexpr float kMuteLookbackPaddingSec = 0.6f;    /* extra padding over token age */
constexpr uint32_t kMaxMuteDelayMs = 10000;        /* sanity cap on UI/external input */
constexpr uint32_t kMaxMutePaddingMs = 5000;

/* ---- Defaults ---- */
constexpr uint32_t kDefaultSampleRate = 48000;

} /* namespace constants */
} /* namespace ai_caption */
