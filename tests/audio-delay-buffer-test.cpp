#include "audio-delay-buffer.h"
#include "test-framework.h"

#include <cmath>
#include <vector>

TEST(BeepGenerator, ProducesSineWave)
{
	BeepGenerator b;
	b.set_params(48000, 1000.0f, 0.5f);
	float sum_sq = 0.0f;
	for (int i = 0; i < 480; i++) {
		float s = b.next();
		sum_sq += s * s;
	}
	const float rms = std::sqrt(sum_sq / 480.0f);
	/* RMS of 0.5-amplitude sine is 0.5 / sqrt(2) ≈ 0.354 */
	EXPECT_NEAR(rms, 0.354, 0.05);
}

TEST(BeepGenerator, ResetsPhaseOnSampleRateChange)
{
	BeepGenerator b;
	b.set_params(48000, 1000.0f, 0.5f);
	(void)b.next();
	b.set_params(16000, 500.0f, 0.3f);
	/* First sample after rate change should be ~0 (phase was reset). */
	const float s = b.next();
	EXPECT_NEAR(s, 0.0, 0.01);
}

TEST(BeepGenerator, GuardsDivideByZeroOnZeroSampleRate)
{
	BeepGenerator b;
	b.set_params(0, 1000.0f, 0.5f);
	/* Should not produce NaN / Inf. Any finite bounded value is OK. */
	for (int i = 0; i < 100; i++) {
		const float s = b.next();
		EXPECT_LE(std::fabs(s), 1.0f);
	}
}

TEST(AudioDelayBuffer, NotConfiguredByDefault)
{
	AudioDelayBuffer d;
	EXPECT_FALSE(d.is_configured());
	EXPECT_EQ(d.get_delay_frames(), 0u);
}

TEST(AudioDelayBuffer, ConfigureSetsDelayFrames)
{
	AudioDelayBuffer d;
	d.configure(48000, 2, 500);
	EXPECT_TRUE(d.is_configured());
	EXPECT_EQ(d.get_delay_frames(), 24000u); /* 48000 * 500 / 1000 */
}

TEST(AudioDelayBuffer, EmitsSilenceBeforeDelayFill)
{
	AudioDelayBuffer d;
	BeepGenerator beep;
	d.configure(48000, 1, 10); /* 480 samples delay */

	std::vector<float> input(240);
	for (size_t i = 0; i < input.size(); i++)
		input[i] = 1.0f;
	float *data[1] = {input.data()};
	std::vector<uint8_t> flags(input.size(), 0);
	d.process(data, static_cast<uint32_t>(input.size()), flags, beep);
	/* Delay line not yet full; output should be zeroed. */
	for (float f : input)
		EXPECT_EQ(f, 0.0f);
}

TEST(AudioDelayBuffer, DelaysAudioAfterFill)
{
	AudioDelayBuffer d;
	BeepGenerator beep;
	d.configure(48000, 1, 10); /* 480 samples delay */

	std::vector<float> chunk1(480);
	for (size_t i = 0; i < chunk1.size(); i++)
		chunk1[i] = 1.0f;
	float *data1[1] = {chunk1.data()};
	std::vector<uint8_t> flags1(chunk1.size(), 0);
	d.process(data1, 480, flags1, beep);
	/* First 480 samples are still silence (delay not yet fully filled
	 * because we need delay_frames BEFORE any real audio comes out). */

	std::vector<float> chunk2(480, 0.0f);
	float *data2[1] = {chunk2.data()};
	std::vector<uint8_t> flags2(chunk2.size(), 0);
	d.process(data2, 480, flags2, beep);
	/* Now chunk2 should contain the delayed 1.0f samples. */
	for (float f : chunk2)
		EXPECT_EQ(f, 1.0f);
}

TEST(AudioDelayBuffer, MuteFlagReplacesWithBeep)
{
	AudioDelayBuffer d;
	BeepGenerator beep;
	beep.set_params(48000, 1000.0f, 0.5f);
	d.configure(48000, 1, 10);

	/* Pre-fill the delay line. */
	std::vector<float> filler(480, 0.5f);
	float *df[1] = {filler.data()};
	std::vector<uint8_t> ff(filler.size(), 0);
	d.process(df, 480, ff, beep);

	/* Now process a chunk with all mute flags set. */
	std::vector<float> chunk(100, 0.1f);
	float *dc[1] = {chunk.data()};
	std::vector<uint8_t> flags(chunk.size(), 1);
	d.process(dc, static_cast<uint32_t>(chunk.size()), flags, beep);

	/* Output should be beep samples (bounded by |volume| = 0.5), not 0.5
	 * delayed samples. At least one sample should deviate from 0.5. */
	bool any_diff = false;
	for (float f : chunk)
		if (std::fabs(f - 0.5f) > 0.05f)
			any_diff = true;
	EXPECT_TRUE(any_diff);
}

TEST(AudioDelayBuffer, ReconfigureResetsStateOnParamChange)
{
	AudioDelayBuffer d;
	BeepGenerator beep;
	d.configure(48000, 1, 10);

	std::vector<float> chunk(240, 1.0f);
	float *data[1] = {chunk.data()};
	std::vector<uint8_t> flags(chunk.size(), 0);
	d.process(data, 240, flags, beep);

	/* Changing delay_ms should reset the buffer. */
	d.configure(48000, 1, 20);
	EXPECT_EQ(d.get_delay_frames(), 960u);

	std::vector<float> chunk2(100, 0.0f);
	float *data2[1] = {chunk2.data()};
	std::vector<uint8_t> flags2(chunk2.size(), 0);
	d.process(data2, 100, flags2, beep);
	/* Buffer was reset, so output is silence (nothing was written yet). */
	for (float f : chunk2)
		EXPECT_EQ(f, 0.0f);
}
