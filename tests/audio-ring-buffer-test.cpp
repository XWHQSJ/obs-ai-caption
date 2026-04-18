#include "audio-ring-buffer.h"
#include "test-framework.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

TEST(AudioRingBuffer, EmptyOnConstruction)
{
	AudioRingBuffer rb(100);
	EXPECT_EQ(rb.size(), 0u);
	EXPECT_EQ(rb.capacity(), 100u);
}

TEST(AudioRingBuffer, WriteAndReadSimple)
{
	AudioRingBuffer rb(100);
	float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
	EXPECT_EQ(rb.write(input, 4), 4u);
	EXPECT_EQ(rb.size(), 4u);

	float output[4] = {};
	EXPECT_EQ(rb.read(output, 4), 4u);
	EXPECT_EQ(output[0], 1.0f);
	EXPECT_EQ(output[1], 2.0f);
	EXPECT_EQ(output[2], 3.0f);
	EXPECT_EQ(output[3], 4.0f);
	EXPECT_EQ(rb.size(), 0u);
}

TEST(AudioRingBuffer, WrapAround)
{
	AudioRingBuffer rb(8);
	float input[6] = {1, 2, 3, 4, 5, 6};
	rb.write(input, 6);

	float out[4] = {};
	rb.read(out, 4);
	EXPECT_EQ(out[3], 4.0f);

	float more[5] = {7, 8, 9, 10, 11};
	EXPECT_EQ(rb.write(more, 5), 5u);
	EXPECT_EQ(rb.size(), 7u); /* 2 leftover + 5 new */

	float final_out[7] = {};
	EXPECT_EQ(rb.read(final_out, 7), 7u);
	EXPECT_EQ(final_out[0], 5.0f);
	EXPECT_EQ(final_out[1], 6.0f);
	EXPECT_EQ(final_out[6], 11.0f);
}

TEST(AudioRingBuffer, WriteDropsOnFull)
{
	AudioRingBuffer rb(10);
	float input[15];
	for (int i = 0; i < 15; i++)
		input[i] = static_cast<float>(i);
	/* capacity=10, one slot reserved -> can hold 9 */
	const size_t written = rb.write(input, 15);
	EXPECT_EQ(written, 9u);
	EXPECT_EQ(rb.size(), 9u);
}

TEST(AudioRingBuffer, ReadReturnsZeroWhenEmpty)
{
	AudioRingBuffer rb(10);
	float out[5];
	EXPECT_EQ(rb.read(out, 5), 0u);
}

TEST(AudioRingBuffer, Clear)
{
	AudioRingBuffer rb(10);
	float in[5] = {1, 2, 3, 4, 5};
	rb.write(in, 5);
	EXPECT_EQ(rb.size(), 5u);
	rb.clear();
	EXPECT_EQ(rb.size(), 0u);
}

TEST(AudioRingBuffer, ShutdownUnblocksWaiter)
{
	AudioRingBuffer rb(10);
	std::atomic<bool> unblocked{false};
	std::thread t([&] {
		rb.wait_for_data(5000);
		unblocked = true;
	});
	/* Give the waiter a moment to actually block. */
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	rb.shutdown();
	t.join();
	EXPECT_TRUE(unblocked.load());
}

TEST(AudioRingBuffer, WaitReturnsImmediatelyIfDataAlreadyPresent)
{
	AudioRingBuffer rb(10);
	float v[2] = {1.0f, 2.0f};
	rb.write(v, 2);
	const auto start = std::chrono::steady_clock::now();
	EXPECT_TRUE(rb.wait_for_data(5000));
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				     std::chrono::steady_clock::now() - start)
				     .count();
	EXPECT_LT(elapsed, 100);
}

TEST(AudioRingBuffer, NoLostWakeupOnRaceBetweenCheckAndWait)
{
	/* Regression test: consumer calls wait_for_data while producer writes
	 * simultaneously. Without proper locking/re-check, the consumer could
	 * block until timeout even though data is available. */
	AudioRingBuffer rb(16);
	std::atomic<bool> consumer_ready{false};
	std::atomic<bool> consumer_got_data{false};
	std::thread consumer([&] {
		consumer_ready = true;
		/* Long timeout - but the producer should wake us up immediately. */
		const bool ok = rb.wait_for_data(5000);
		consumer_got_data = ok;
	});
	while (!consumer_ready.load())
		std::this_thread::yield();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	float v[1] = {42.0f};
	rb.write(v, 1);
	consumer.join();
	EXPECT_TRUE(consumer_got_data.load());
}

TEST(AudioRingBuffer, SPSCStressOrderPreserved)
{
	AudioRingBuffer rb(1024);
	constexpr size_t total = 50000;
	std::vector<float> received;
	received.reserve(total);

	std::thread producer([&] {
		size_t written = 0;
		size_t i = 0;
		while (written < total) {
			float v = static_cast<float>(i);
			size_t n = rb.write(&v, 1);
			if (n == 1) {
				written++;
				i++;
			} else {
				std::this_thread::yield();
			}
		}
	});

	std::thread consumer([&] {
		while (received.size() < total) {
			float v = 0;
			if (rb.read(&v, 1) == 1)
				received.push_back(v);
			else
				rb.wait_for_data(1);
		}
	});

	producer.join();
	consumer.join();

	ASSERT_EQ(received.size(), total);
	for (size_t i = 0; i < total; i++) {
		if (received[i] != static_cast<float>(i)) {
			FAIL("out-of-order sample");
			break;
		}
	}
}
