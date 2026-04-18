#include "audio-analyzer.h"
#include "test-framework.h"

#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

std::vector<float> sine_wave(float freq, uint32_t sample_rate, size_t frames, float amp = 0.5f)
{
	std::vector<float> out(frames);
	for (size_t i = 0; i < frames; i++) {
		double t = static_cast<double>(i) / static_cast<double>(sample_rate);
		out[i] = amp * static_cast<float>(std::sin(2.0 * M_PI * freq * t));
	}
	return out;
}

} /* namespace */

TEST(AudioAnalyzer, SilenceHasLowRms)
{
	AudioAnalyzer a;
	std::vector<float> silence(4800, 0.0f);
	a.feed(silence.data(), silence.size(), 48000);
	EXPECT_LT(a.get_rms(), 1e-3f);
}

TEST(AudioAnalyzer, DetectsSineFrequency)
{
	AudioAnalyzer a;
	const uint32_t sr = 48000;
	const float target_hz = 200.0f;
	/* Feed ~300ms of a 200Hz sine; analyzer should converge within
	 * kPitchSmoothingAlpha iterations. */
	for (int iter = 0; iter < 30; iter++) {
		auto buf = sine_wave(target_hz, sr, 480);
		a.feed(buf.data(), buf.size(), sr);
	}
	/* Detection tolerance: +/- 15Hz is generous for normalized
	 * autocorrelation with 30ms window. */
	EXPECT_NEAR(a.get_freq(), target_hz, 15.0);
}

TEST(AudioAnalyzer, RmsRisesWithSignal)
{
	AudioAnalyzer a;
	const uint32_t sr = 48000;
	/* Feed repeatedly so the EMA has time to converge. */
	for (int i = 0; i < 20; i++) {
		auto buf = sine_wave(440.0f, sr, 480, 0.8f);
		a.feed(buf.data(), buf.size(), sr);
	}
	EXPECT_GT(a.get_rms(), 0.1f);
}

TEST(AudioAnalyzer, ReconfiguresOnSampleRateChange)
{
	AudioAnalyzer a;
	auto buf48 = sine_wave(300.0f, 48000, 4800);
	a.feed(buf48.data(), buf48.size(), 48000);

	auto buf16 = sine_wave(300.0f, 16000, 1600);
	a.feed(buf16.data(), buf16.size(), 16000);
	/* Should not crash; rms should reflect recent signal. */
	EXPECT_GT(a.get_rms(), 0.0f);
}

TEST(AudioAnalyzer, ResetRestoresDefaults)
{
	AudioAnalyzer a;
	auto buf = sine_wave(440.0f, 48000, 4800, 0.8f);
	a.feed(buf.data(), buf.size(), 48000);
	EXPECT_GT(a.get_rms(), 0.0f);
	a.reset();
	EXPECT_EQ(a.get_rms(), 0.0f);
}
