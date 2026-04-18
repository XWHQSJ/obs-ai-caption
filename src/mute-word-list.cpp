#include "mute-word-list.h"

#include <cctype>
#include <fstream>
#include <utility>

namespace {

inline bool is_ascii_alnum(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool is_ascii_only(const std::string &s)
{
	for (unsigned char c : s) {
		if (c >= 0x80)
			return false;
	}
	return true;
}

void trim_in_place(std::string &s)
{
	const size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		s.clear();
		return;
	}
	const size_t end = s.find_last_not_of(" \t\r\n");
	s = s.substr(start, end - start + 1);
}

void to_lower_in_place(std::string &s)
{
	for (auto &c : s) {
		unsigned char uc = static_cast<unsigned char>(c);
		if (uc < 0x80)
			c = static_cast<char>(std::tolower(uc));
	}
}

/* Count non-overlapping occurrences of `needle` in `haystack`. When
 * `require_word_boundary` is true, the character immediately before and the
 * character immediately after the match must be non-[a-z0-9] (or be at the
 * string edge). */
int count_substr(const std::string &haystack, const std::string &needle, bool require_word_boundary)
{
	if (needle.empty() || haystack.size() < needle.size())
		return 0;

	int count = 0;
	size_t pos = 0;
	while ((pos = haystack.find(needle, pos)) != std::string::npos) {
		bool ok = true;
		if (require_word_boundary) {
			const bool left_boundary =
				(pos == 0) ||
				!is_ascii_alnum(static_cast<unsigned char>(haystack[pos - 1]));
			const size_t after = pos + needle.size();
			const bool right_boundary =
				(after >= haystack.size()) ||
				!is_ascii_alnum(static_cast<unsigned char>(haystack[after]));
			ok = left_boundary && right_boundary;
		}
		if (ok) {
			count++;
			pos += needle.size();
		} else {
			pos += 1;
		}
	}
	return count;
}

} /* namespace */

void MuteWordList::load(const std::string &path)
{
	std::vector<Word> tmp_words;
	std::string tmp_buf;

	if (path.empty()) {
		std::lock_guard<std::mutex> lk(mutex_);
		words_.clear();
		hotwords_buf_.clear();
		return;
	}

	std::ifstream in(path);
	if (!in.is_open())
		return; /* keep current state on failure */

	std::string line;
	while (std::getline(in, line)) {
		trim_in_place(line);
		if (line.empty() || line[0] == '#')
			continue;

		tmp_buf += line;
		tmp_buf += '\n';

		std::string word = line;
		const size_t colon = line.rfind(':');
		if (colon != std::string::npos && colon > 0) {
			const size_t before = line.find_last_not_of(" \t", colon - 1);
			if (before != std::string::npos)
				word = line.substr(0, before + 1);
		}
		if (word.empty())
			continue;

		to_lower_in_place(word);
		Word w{word, is_ascii_only(word)};
		tmp_words.emplace_back(std::move(w));
	}

	std::lock_guard<std::mutex> lk(mutex_);
	words_ = std::move(tmp_words);
	hotwords_buf_ = std::move(tmp_buf);
}

int MuteWordList::count_matches_locked(const std::string &text_lower) const
{
	int total = 0;
	for (const auto &w : words_) {
		if (w.lower.empty())
			continue;
		total += count_substr(text_lower, w.lower, w.ascii_only);
	}
	return total;
}

int MuteWordList::count_matches(const std::string &text) const
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (words_.empty())
		return 0;
	std::string lower = text;
	to_lower_in_place(lower);
	return count_matches_locked(lower);
}

std::string MuteWordList::get_hotwords_buf() const
{
	std::lock_guard<std::mutex> lk(mutex_);
	return hotwords_buf_;
}

bool MuteWordList::empty() const
{
	std::lock_guard<std::mutex> lk(mutex_);
	return words_.empty();
}
