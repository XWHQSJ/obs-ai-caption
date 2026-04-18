#pragma once

#include "audio-ring-buffer.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

/* -------------------------------------------------------- */
/* Configuration                                            */

struct AsrConfig {
	std::string encoder_path;
	std::string decoder_path;
	std::string joiner_path;
	std::string tokens_path;
	std::string bpe_vocab_path;

	int num_threads = 2;
	std::string provider = "cpu";

	int sample_rate = 16000;
	int feature_dim = 80;
	std::string decoding_method = "greedy_search";
	std::string modeling_unit; /* empty by default - must match caller's intent */
	int max_active_paths = 4;

	bool enable_endpoint = true;
	float rule1_min_trailing_silence = 2.4f;
	float rule2_min_trailing_silence = 1.2f;
	float rule3_min_utterance_length = 20.0f;

	std::string hotwords_file;
	std::string hotwords_buf;
	float hotwords_score = 1.5f;
};

struct AsrSegment {
	std::string text;
	bool is_endpoint;
};

struct AsrStats {
	uint64_t dropped_samples = 0;
	uint64_t decode_errors = 0;
};

using AsrResultCallback =
	std::function<void(const std::string &text, bool is_partial, float token_age_sec)>;

/* -------------------------------------------------------- */
/* ASR Engine                                               */

class AsrEngine {
public:
	AsrEngine();
	~AsrEngine();

	AsrEngine(const AsrEngine &) = delete;
	AsrEngine &operator=(const AsrEngine &) = delete;

	bool init(const AsrConfig &config);
	void shutdown();

	/* The callback is invoked from the internal decode thread, while the
	 * engine still holds the onnx mutex. Implementations must NOT call
	 * back into this engine from within the callback. */
	void set_result_callback(AsrResultCallback cb);

	void feed_audio(const float *samples, size_t count, int sample_rate);
	void reset();

	AsrStats get_stats() const;
	void reset_stats();

	/* Pending samples still queued for decoding (backlog in ring buffer). */
	size_t get_pending_samples() const { return ring_.size(); }

	/* Deprecated: retained for backward compatibility. Prefer
	 * get_pending_samples() which is less ambiguous with "capacity". */
	size_t get_ring_size() const { return ring_.size(); }

	bool is_running() const { return running_.load(std::memory_order_acquire); }

private:
	void decode_thread_func();

	struct Impl;
	Impl *impl_ = nullptr;

	AudioRingBuffer ring_;
	std::atomic<int> source_sample_rate_{48000};

	std::thread decode_thread_;
	std::atomic<bool> running_{false};
	std::atomic<bool> shutting_down_{false};
	std::atomic<bool> should_reset_{false};

	std::mutex onnx_mutex_;

	std::mutex callback_mutex_;
	AsrResultCallback result_callback_;

	std::atomic<uint64_t> dropped_samples_{0};
	std::atomic<uint64_t> decode_errors_{0};
	std::string last_text_;
};
