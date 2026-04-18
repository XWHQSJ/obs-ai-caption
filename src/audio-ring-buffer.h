#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

/* Lock-free SPSC ring buffer for float audio samples.
 *
 * Single-producer (OBS audio thread) / single-consumer (ASR decode thread).
 * Bounded capacity; writes beyond capacity are dropped and the caller can
 * observe the shortfall via the return value of write().
 *
 * Thread-safety: one thread calls write(); one thread calls read() and
 * wait_for_data(). size() / shutdown() / clear() / reset() are safe from
 * any thread at any time.
 */
class AudioRingBuffer {
public:
	explicit AudioRingBuffer(size_t capacity);
	~AudioRingBuffer() = default;

	AudioRingBuffer(const AudioRingBuffer &) = delete;
	AudioRingBuffer &operator=(const AudioRingBuffer &) = delete;

	/* Returns number of samples actually written (<= count). */
	size_t write(const float *data, size_t count);

	/* Returns number of samples actually read (<= count). */
	size_t read(float *dest, size_t count);

	void clear();
	void shutdown();
	void reset();

	size_t size() const;
	size_t capacity() const { return capacity_; }
	bool wait_for_data(int timeout_ms);

private:
	std::vector<float> buf_;
	size_t capacity_;
	std::atomic<size_t> read_pos_{0};
	std::atomic<size_t> write_pos_{0};

	std::mutex cv_mutex_;
	std::condition_variable not_empty_;
	std::atomic<bool> shutdown_{false};
};
