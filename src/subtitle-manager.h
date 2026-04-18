#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct SubtitleSnapshot {
	std::vector<std::string> finals;
	std::string partial;
};

class SubtitleManager {
public:
	void set_max_lines(int value);
	void set_clear_timeout_sec(double value);

	void on_partial(const std::string &text, uint64_t now_ms);
	void on_final(const std::string &text, uint64_t now_ms);

	SubtitleSnapshot snapshot(uint64_t now_ms) const;
	std::string compose_output(uint64_t now_ms) const;
	void set_partial_write_throttle_ms(uint64_t value);
	bool should_emit_file_update(uint64_t now_ms);

private:
	void trim_history();

	int max_lines_ = 2;
	double clear_timeout_sec_ = 5.0;
	uint64_t last_text_time_ms_ = 0;
	uint64_t partial_write_throttle_ms_ = 0;
	uint64_t last_partial_write_ms_ = 0;
	std::string last_emitted_content_;
	bool has_pending_partial_update_ = false;
	bool force_emit_next_update_ = false;
	std::deque<std::string> finals_;
	std::string partial_;
};
