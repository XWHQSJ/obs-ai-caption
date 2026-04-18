#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/* Sine-wave beep generator. The phase is preserved across chunks so that
 * continuous beeping sounds stable; call set_params() once per chunk when the
 * parameters change. */
class BeepGenerator {
public:
	void set_params(uint32_t sample_rate, float freq, float volume);
	float next();

private:
	uint32_t sample_rate_ = 48000;
	float freq_ = 1000.0f;
	float volume_ = 0.5f;
	double phase_ = 0.0;
	double phase_inc_ = 0.0;
};

/* Fixed-length delay line for multi-channel planar float audio with a
 * per-sample mute gate.
 *
 * Each call to process() reads the delayed samples (delay_frames worth) into
 * the output, while writing the current input into the ring. Where
 * mute_flags[i] is non-zero, the output is replaced by the beep generator's
 * next sample instead of the delayed audio.
 *
 * Reconfiguration semantics: any change to sample_rate, channels, or delay_ms
 * resets the internal state to avoid leaking stale audio frames. */
class AudioDelayBuffer {
public:
	void configure(uint32_t sample_rate, size_t channels, uint32_t delay_ms);
	bool is_configured() const { return delay_frames_ > 0 && channels_ > 0; }
	size_t get_delay_frames() const { return delay_frames_; }

	void process(float **channel_data, uint32_t frames,
		     const std::vector<uint8_t> &mute_flags, BeepGenerator &beep);

	void reset();

private:
	std::vector<float> buf_;
	uint32_t sample_rate_ = 0;
	size_t channels_ = 0;
	uint32_t delay_ms_ = 0;
	size_t delay_frames_ = 0;
	size_t write_pos_ = 0;
	size_t filled_ = 0;
};
