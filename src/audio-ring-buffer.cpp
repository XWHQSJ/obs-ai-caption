#include "audio-ring-buffer.h"

#include <algorithm>
#include <chrono>
#include <cstring>

AudioRingBuffer::AudioRingBuffer(size_t capacity)
	: buf_(capacity), capacity_(capacity)
{
}

size_t AudioRingBuffer::write(const float *data, size_t count)
{
	if (count == 0 || shutdown_.load(std::memory_order_relaxed))
		return 0;

	const size_t current_write = write_pos_.load(std::memory_order_relaxed);
	const size_t current_read = read_pos_.load(std::memory_order_acquire);

	/* Available space = capacity - used - 1 (one slot reserved to distinguish
	   empty from full). */
	size_t space;
	if (current_write >= current_read)
		space = capacity_ - current_write + current_read - 1;
	else
		space = current_read - current_write - 1;

	const size_t n = std::min(count, space);
	if (n == 0)
		return 0;

	const size_t first = std::min(n, capacity_ - current_write);
	std::memcpy(buf_.data() + current_write, data, first * sizeof(float));
	if (first < n)
		std::memcpy(buf_.data(), data + first, (n - first) * sizeof(float));

	write_pos_.store((current_write + n) % capacity_, std::memory_order_release);
	not_empty_.notify_one();
	return n;
}

size_t AudioRingBuffer::read(float *dest, size_t count)
{
	if (count == 0)
		return 0;

	const size_t current_read = read_pos_.load(std::memory_order_relaxed);
	const size_t current_write = write_pos_.load(std::memory_order_acquire);

	size_t available;
	if (current_write >= current_read)
		available = current_write - current_read;
	else
		available = capacity_ - current_read + current_write;

	const size_t n = std::min(count, available);
	if (n == 0)
		return 0;

	const size_t first = std::min(n, capacity_ - current_read);
	std::memcpy(dest, buf_.data() + current_read, first * sizeof(float));
	if (first < n)
		std::memcpy(dest + first, buf_.data(), (n - first) * sizeof(float));

	read_pos_.store((current_read + n) % capacity_, std::memory_order_release);
	return n;
}

void AudioRingBuffer::clear()
{
	read_pos_.store(0, std::memory_order_relaxed);
	write_pos_.store(0, std::memory_order_relaxed);
}

void AudioRingBuffer::shutdown()
{
	shutdown_.store(true, std::memory_order_relaxed);
	not_empty_.notify_all();
}

void AudioRingBuffer::reset()
{
	shutdown_.store(false, std::memory_order_relaxed);
	clear();
}

size_t AudioRingBuffer::size() const
{
	const size_t current_read = read_pos_.load(std::memory_order_acquire);
	const size_t current_write = write_pos_.load(std::memory_order_acquire);
	if (current_write >= current_read)
		return current_write - current_read;
	return capacity_ - current_read + current_write;
}

bool AudioRingBuffer::wait_for_data(int timeout_ms)
{
	/* Fast path: no need to acquire the CV mutex if data is already
	 * available or we're shutting down. Both are atomic loads with the
	 * appropriate acquire semantics. */
	if (size() > 0 || shutdown_.load(std::memory_order_acquire))
		return true;

	std::unique_lock<std::mutex> lock(cv_mutex_);
	/* Re-check predicate under the lock. wait_for's predicate form is
	 * equivalent to while (!pred()) wait_for(...), which combines with the
	 * mutex to guarantee we won't miss a notify_one() issued by a producer
	 * between our fast-path check and the wait. */
	return not_empty_.wait_for(
		lock, std::chrono::milliseconds(timeout_ms),
		[this] { return size() > 0 || shutdown_.load(std::memory_order_relaxed); });
}
