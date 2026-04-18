#include "subtitle-manager.h"
#include "test-framework.h"

TEST(SubtitleManager, StartsEmpty)
{
	SubtitleManager mgr;
	auto snap = mgr.snapshot(1000);
	EXPECT_TRUE(snap.finals.empty());
	EXPECT_TRUE(snap.partial.empty());
}

TEST(SubtitleManager, PartialThenFinal)
{
	SubtitleManager mgr;
	mgr.set_max_lines(2);
	mgr.on_partial("hello", 100);
	auto snap = mgr.snapshot(100);
	EXPECT_STREQ(snap.partial, "hello");
	EXPECT_EQ(snap.finals.size(), 0u);

	mgr.on_final("hello world", 200);
	snap = mgr.snapshot(200);
	EXPECT_STREQ(snap.partial, "");
	ASSERT_EQ(snap.finals.size(), 1u);
	EXPECT_STREQ(snap.finals[0], "hello world");
}

TEST(SubtitleManager, MaxLinesTrim)
{
	SubtitleManager mgr;
	mgr.set_max_lines(2);
	mgr.on_final("line1", 100);
	mgr.on_final("line2", 101);
	mgr.on_final("line3", 102);
	auto snap = mgr.snapshot(102);
	ASSERT_EQ(snap.finals.size(), 2u);
	EXPECT_STREQ(snap.finals[0], "line2");
	EXPECT_STREQ(snap.finals[1], "line3");
}

TEST(SubtitleManager, ClearTimeoutClearsSnapshot)
{
	SubtitleManager mgr;
	mgr.set_max_lines(2);
	mgr.set_clear_timeout_sec(1.0);
	mgr.on_final("fresh", 100);

	auto still_visible = mgr.snapshot(500); /* 400ms later */
	EXPECT_EQ(still_visible.finals.size(), 1u);

	auto gone = mgr.snapshot(2000); /* 1.9s later — past timeout */
	EXPECT_EQ(gone.finals.size(), 0u);
}

TEST(SubtitleManager, ComposeJoinsWithNewlines)
{
	SubtitleManager mgr;
	mgr.set_max_lines(3);
	mgr.on_final("line1", 100);
	mgr.on_final("line2", 101);
	mgr.on_partial("pending", 102);
	auto out = mgr.compose_output(102);
	EXPECT_STREQ(out, "line1\nline2\npending\n");
}

TEST(SubtitleManager, ThrottlesPartialWrites)
{
	SubtitleManager mgr;
	mgr.set_partial_write_throttle_ms(80);
	mgr.on_partial("a", 100);
	EXPECT_TRUE(mgr.should_emit_file_update(100));
	/* Same content → no update */
	EXPECT_FALSE(mgr.should_emit_file_update(120));
	mgr.on_partial("ab", 130);
	/* Throttled: 130 - 100 = 30ms < 80ms */
	EXPECT_FALSE(mgr.should_emit_file_update(130));
	/* Past the throttle window */
	EXPECT_TRUE(mgr.should_emit_file_update(200));
}

TEST(SubtitleManager, ShouldEmitThenComposeGivesSameContent)
{
	/* Regression: should_emit_file_update(t) updates internal state so
	 * that calling compose_output(t) afterwards must return the *same*
	 * content that was deemed new. If a caller composes after checking,
	 * the file should end up with that content. */
	SubtitleManager mgr;
	mgr.on_partial("abc", 100);
	EXPECT_TRUE(mgr.should_emit_file_update(100));
	const std::string composed = mgr.compose_output(100);
	EXPECT_STREQ(composed, "abc\n");
	/* A second should_emit call without content change must be false. */
	EXPECT_FALSE(mgr.should_emit_file_update(100));
}

TEST(SubtitleManager, FinalForcesImmediateEmit)
{
	SubtitleManager mgr;
	mgr.set_partial_write_throttle_ms(80);
	mgr.on_partial("a", 100);
	(void)mgr.should_emit_file_update(100);
	mgr.on_final("hello", 110);
	/* Must emit even within throttle window because final forces it. */
	EXPECT_TRUE(mgr.should_emit_file_update(110));
}
