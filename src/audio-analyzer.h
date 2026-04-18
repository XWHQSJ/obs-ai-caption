#pragma once

#include <cstdint>
#include <vector>

/* Real-time audio analyzer: tracks smoothed RMS and fundamental frequency (F0)
 * via autocorrelation over a sliding window.
 *
 * NOT thread-safe. Feed and read from the same thread (OBS audio callback).
 */
class AudioAnalyzer {
public:
	void feed(const float *mono, size_t frames, uint32_t sample_rate);

	float get_rms() const { return rms_; }
	float get_freq() const { return freq_; }

	/* Visible for tests. */
	void reset();

private:
	void configure_for_rate(uint32_t sample_rate);
	void detect_pitch();

	float rms_ = 0.0f;
	float freq_ = 200.0f;
	uint32_t sample_rate_ = 0;

	std::vector<float> pitch_buf_;
	std::vector<float> linear_buf_;
	size_t window_size_ = 0;
	size_t pitch_write_ = 0;
	size_t update_counter_ = 0;
	size_t min_lag_ = 0;
	size_t max_lag_ = 0;
};
