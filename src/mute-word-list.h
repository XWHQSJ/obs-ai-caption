#pragma once

#include <mutex>
#include <string>
#include <vector>

/* Sensitive word list for mute/beep replacement.
 *
 * File format (one per line, sherpa-onnx hotwords format):
 *   fuck :5.0
 *   shit :4.0
 *   damn :3.0
 *
 * Lines starting with '#' are comments. The score suffix (after ':') is
 * preserved for the hotwords_buf but stripped from the matching dictionary.
 *
 * Matching rules:
 *   - Patterns are case-insensitive.
 *   - If pattern contains non-ASCII bytes (e.g., Chinese/Japanese), it is
 *     matched as a substring with no boundary check.
 *   - If pattern is pure ASCII alphanumeric, it is matched with word
 *     boundaries: the characters immediately before and after the match
 *     must be non-[a-z0-9] (or string edges).
 *
 * Thread-safe: all public methods acquire an internal mutex.
 */
class MuteWordList {
public:
	void load(const std::string &path);

	/* Returns number of distinct in-text occurrences across all patterns. */
	int count_matches(const std::string &text) const;

	std::string get_hotwords_buf() const;
	bool empty() const;

private:
	struct Word {
		std::string lower; /* lowercased pattern used for matching */
		bool ascii_only;   /* true → apply word-boundary; false → plain substring */
	};

	int count_matches_locked(const std::string &text_lower) const;

	mutable std::mutex mutex_;
	std::vector<Word> words_;
	std::string hotwords_buf_;
};
