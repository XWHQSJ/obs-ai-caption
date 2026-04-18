#include "mute-word-list.h"
#include "test-framework.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

std::string write_temp_list(const std::string &content)
{
#ifdef _WIN32
	char buf[L_tmpnam];
	std::tmpnam(buf);
	std::string path = buf;
	path += ".txt";
#else
	std::string path = "/tmp/mute-word-list-test-";
	path += std::to_string(static_cast<unsigned long long>(std::rand()));
	path += ".txt";
#endif
	std::ofstream out(path);
	out << content;
	return path;
}

} /* namespace */

TEST(MuteWordList, EmptyPathClears)
{
	MuteWordList list;
	list.load("");
	EXPECT_TRUE(list.empty());
	EXPECT_EQ(list.count_matches("anything"), 0);
}

TEST(MuteWordList, MatchesAsciiWithWordBoundary)
{
	MuteWordList list;
	const auto path = write_temp_list("fuck :5.0\n");
	list.load(path);
	std::remove(path.c_str());
	EXPECT_FALSE(list.empty());
	EXPECT_EQ(list.count_matches("what the fuck man"), 1);
	EXPECT_EQ(list.count_matches("WHAT THE FUCK MAN"), 1);
	EXPECT_EQ(list.count_matches("fucking"), 0); /* boundary rejected */
	EXPECT_EQ(list.count_matches("motherfuck"), 0);
	EXPECT_EQ(list.count_matches("fuck!"), 1);
	EXPECT_EQ(list.count_matches("fuck"), 1);
	EXPECT_EQ(list.count_matches(""), 0);
}

TEST(MuteWordList, CountsMultipleOccurrences)
{
	MuteWordList list;
	const auto path = write_temp_list("shit :4.0\n");
	list.load(path);
	std::remove(path.c_str());
	EXPECT_EQ(list.count_matches("shit shit shit"), 3);
}

TEST(MuteWordList, WordBoundaryAtStartAndEnd)
{
	MuteWordList list;
	const auto path = write_temp_list("damn :3.0\n");
	list.load(path);
	std::remove(path.c_str());
	EXPECT_EQ(list.count_matches("damn"), 1);
	EXPECT_EQ(list.count_matches("damn!"), 1);
	EXPECT_EQ(list.count_matches("!damn"), 1);
	EXPECT_EQ(list.count_matches(" damn "), 1);
	EXPECT_EQ(list.count_matches("godamn"), 0);
	EXPECT_EQ(list.count_matches("damnit"), 0);
	/* The prior bug would underflow size_t on pos=0; confirm no regression
	 * on a string that starts with the match. "damn!damn" has two matches:
	 * both have boundaries ('!' is non-alnum). */
	EXPECT_EQ(list.count_matches("damn!damn"), 2);
	/* Two consecutive pattern bytes with an alnum separator are NOT a word
	 * boundary: "damnxdamn" -> 0 matches. */
	EXPECT_EQ(list.count_matches("damnxdamn"), 0);
}

TEST(MuteWordList, ChineseSubstringMatch)
{
	/* Non-ASCII patterns use plain substring matching (no boundary). */
	MuteWordList list;
	const auto path = write_temp_list("草 :3.0\n");
	list.load(path);
	std::remove(path.c_str());
	EXPECT_EQ(list.count_matches("草原"), 1);
	EXPECT_EQ(list.count_matches("你草了没"), 1);
	EXPECT_EQ(list.count_matches("无关内容"), 0);
}

TEST(MuteWordList, IgnoresCommentsAndBlankLines)
{
	MuteWordList list;
	const auto path = write_temp_list("# this is a comment\n\nfuck :5.0\n   \n# another\n");
	list.load(path);
	std::remove(path.c_str());
	EXPECT_EQ(list.count_matches("fuck"), 1);
}

TEST(MuteWordList, HotwordsBufPreservesScoreFormat)
{
	MuteWordList list;
	const auto path = write_temp_list("fuck :5.0\nshit :4.0\n");
	list.load(path);
	std::remove(path.c_str());
	const auto buf = list.get_hotwords_buf();
	/* Both lines should be present */
	EXPECT_NE(buf.find("fuck :5.0"), std::string::npos);
	EXPECT_NE(buf.find("shit :4.0"), std::string::npos);
}

TEST(MuteWordList, ReloadReplacesOldWords)
{
	MuteWordList list;
	auto path1 = write_temp_list("foo :1.0\n");
	list.load(path1);
	std::remove(path1.c_str());
	EXPECT_EQ(list.count_matches("foo"), 1);

	auto path2 = write_temp_list("bar :1.0\n");
	list.load(path2);
	std::remove(path2.c_str());
	EXPECT_EQ(list.count_matches("foo"), 0);
	EXPECT_EQ(list.count_matches("bar"), 1);
}
