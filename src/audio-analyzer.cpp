#include "audio-analyzer.h"
#include "constants.h"

#include <algorithm>
#include <cmath>

using namespace ai_caption::constants;

void AudioAnalyzer::reset()
{
	rms_ = 0.0f;
	freq_ = 200.0f;
	sample_rate_ = 0;
	pitch_buf_.clear();
	linear_buf_.clear();
	window_size_ = 0;
	pitch_write_ = 0;
	update_counter_ = 0;
	min_lag_ = 0;
	max_lag_ = 0;
}

void AudioAnalyzer::configure_for_rate(uint32_t sample_rate)
{
	sample_rate_ = sample_rate;
	window_size_ = static_cast<size_t>(sample_rate) * kAnalyzerWindowMs / 1000;
	if (window_size_ < 2)
		window_size_ = 2;
	pitch_buf_.assign(window_size_, 0.0f);
	linear_buf_.assign(window_size_, 0.0f);
	pitch_write_ = 0;
	update_counter_ = 0;
	min_lag_ = static_cast<size_t>(sample_rate / kMaxPitchHz);
	max_lag_ = static_cast<size_t>(sample_rate / kMinPitchHz);
	if (min_lag_ < 1)
		min_lag_ = 1;
	if (max_lag_ >= window_size_)
		max_lag_ = window_size_ - 1;
}

void AudioAnalyzer::feed(const float *mono, size_t frames, uint32_t sample_rate)
{
	if (frames == 0 || !mono)
		return;

	if (sample_rate != sample_rate_ || pitch_buf_.empty())
		configure_for_rate(sample_rate);

	/* RMS: exponential moving average */
	float sum_sq = 0.0f;
	for (size_t i = 0; i < frames; i++)
		sum_sq += mono[i] * mono[i];
	const float frame_rms = std::sqrt(sum_sq / static_cast<float>(std::max<size_t>(frames, 1)));
	rms_ = rms_ * (1.0f - kRmsSmoothingAlpha) + frame_rms * kRmsSmoothingAlpha;

	/* Fill circular pitch analysis buffer */
	for (size_t i = 0; i < frames; i++) {
		pitch_buf_[pitch_write_] = mono[i];
		pitch_write_ = (pitch_write_ + 1) % window_size_;
	}

	update_counter_ += frames;
	const size_t interval =
		sample_rate_ / static_cast<size_t>(kPitchUpdateDivisor);
	if (interval > 0 && update_counter_ >= interval) {
		update_counter_ = 0;
		detect_pitch();
	}
}

void AudioAnalyzer::detect_pitch()
{
	if (min_lag_ >= max_lag_ || window_size_ < max_lag_ + 1)
		return;

	/* Linearize the circular buffer */
	for (size_t i = 0; i < window_size_; i++)
		linear_buf_[i] = pitch_buf_[(pitch_write_ + i) % window_size_];

	float energy = 0.0f;
	for (size_t i = 0; i < window_size_; i++)
		energy += linear_buf_[i] * linear_buf_[i];

	if (energy < 1e-8f)
		return;

	/* Normalized autocorrelation: strongest peak within voice lag range. */
	float best_corr = 0.0f;
	size_t best_lag = 0;
	for (size_t lag = min_lag_; lag <= max_lag_; lag++) {
		float corr = 0.0f;
		const size_t n = window_size_ - lag;
		for (size_t i = 0; i < n; i++)
			corr += linear_buf_[i] * linear_buf_[i + lag];
		corr /= energy;
		if (corr > best_corr) {
			best_corr = corr;
			best_lag = lag;
		}
	}

	if (best_corr > kPitchCorrelationThreshold && best_lag > 0) {
		const float detected = static_cast<float>(sample_rate_) / static_cast<float>(best_lag);
		freq_ = freq_ * (1.0f - kPitchSmoothingAlpha) + detected * kPitchSmoothingAlpha;
	}
}
