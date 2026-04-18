#pragma once

/* Minimal header-only test framework.
 *
 * Usage:
 *     #include "test-framework.h"
 *     TEST(group, name) { EXPECT_EQ(1 + 1, 2); }
 *     int main() { return test_framework::run_all(); }
 *
 * Macros:
 *     TEST(group, name) { ... }       - register a test
 *     EXPECT_TRUE(cond)               - non-fatal boolean check
 *     EXPECT_FALSE(cond)
 *     EXPECT_EQ(a, b)
 *     EXPECT_NE(a, b)
 *     EXPECT_LT(a, b) / LE / GT / GE
 *     EXPECT_NEAR(a, b, tol)          - floating-point approx equality
 *     EXPECT_STREQ(a, b)              - string equality
 *     ASSERT_*                         - like EXPECT_*, but aborts the test
 *     FAIL(msg)                       - unconditional failure
 *
 * No external dependencies; just <cstdio>, <cstdlib>, <cmath>, <string>,
 * <vector>, <functional>. */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace test_framework {

struct TestCase {
	const char *group;
	const char *name;
	std::function<void(bool & /*failed_flag*/, bool & /*aborted_flag*/)> fn;
};

inline std::vector<TestCase> &registry()
{
	static std::vector<TestCase> tests;
	return tests;
}

struct Registrar {
	Registrar(const char *group, const char *name,
		  std::function<void(bool &, bool &)> fn)
	{
		registry().push_back({group, name, std::move(fn)});
	}
};

inline int run_all()
{
	int passed = 0;
	int failed = 0;
	for (const auto &t : registry()) {
		bool f = false;
		bool aborted = false;
		std::printf("[ RUN      ] %s.%s\n", t.group, t.name);
		try {
			t.fn(f, aborted);
		} catch (const std::exception &e) {
			std::printf("    throw std::exception: %s\n", e.what());
			f = true;
		} catch (...) {
			std::printf("    throw unknown exception\n");
			f = true;
		}
		if (f) {
			std::printf("[  FAILED  ] %s.%s\n", t.group, t.name);
			failed++;
		} else {
			std::printf("[       OK ] %s.%s\n", t.group, t.name);
			passed++;
		}
	}
	std::printf("\n%d passed, %d failed (of %d tests)\n", passed, failed,
		    passed + failed);
	return failed == 0 ? 0 : 1;
}

} /* namespace test_framework */

/* ---------------- Macros ---------------- */

#define TF_REG_NAME2(g, n) tf_reg_##g##_##n
#define TF_REG_NAME(g, n) TF_REG_NAME2(g, n)

#define TEST(group, name)                                                  \
	static void tf_##group##_##name##_body(bool &tf_failed, bool &tf_abort); \
	static ::test_framework::Registrar TF_REG_NAME(group, name){            \
		#group, #name,                                                \
		[](bool &f, bool &a) { tf_##group##_##name##_body(f, a); }}; \
	static void tf_##group##_##name##_body([[maybe_unused]] bool &tf_failed, \
					       [[maybe_unused]] bool &tf_abort)

#define TF_PRINT_LOC()                                                      \
	std::printf("    %s:%d: failure\n", __FILE__, __LINE__)

#define EXPECT_TRUE(cond)                                                  \
	do {                                                               \
		if (!(cond)) {                                             \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_TRUE(%s) failed\n",      \
				    #cond);                                \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va == _vb)) {                                       \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_EQ(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_NE(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va != _vb)) {                                       \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_NE(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_LT(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va < _vb)) {                                        \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_LT(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_LE(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va <= _vb)) {                                       \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_LE(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_GT(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va > _vb)) {                                        \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_GT(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_GE(a, b)                                                    \
	do {                                                               \
		auto _va = (a);                                            \
		auto _vb = (b);                                            \
		if (!(_va >= _vb)) {                                       \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_GE(%s, %s) failed\n",    \
				    #a, #b);                               \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_NEAR(a, b, tol)                                             \
	do {                                                               \
		double _va = static_cast<double>(a);                       \
		double _vb = static_cast<double>(b);                       \
		if (std::fabs(_va - _vb) > (tol)) {                        \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_NEAR(%s, %s, %s) "       \
				    "failed (|%.6g - %.6g| > %.6g)\n",     \
				    #a, #b, #tol, _va, _vb,                \
				    static_cast<double>(tol));             \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define EXPECT_STREQ(a, b)                                                 \
	do {                                                               \
		std::string _sa(a);                                        \
		std::string _sb(b);                                        \
		if (_sa != _sb) {                                          \
			TF_PRINT_LOC();                                    \
			std::printf("      EXPECT_STREQ(%s, %s) failed: "  \
				    "\"%s\" vs \"%s\"\n",                  \
				    #a, #b, _sa.c_str(), _sb.c_str());     \
			tf_failed = true;                                  \
		}                                                          \
	} while (0)

#define ASSERT_TRUE(cond)                                                  \
	do {                                                               \
		EXPECT_TRUE(cond);                                         \
		if (tf_failed) {                                           \
			tf_abort = true;                                   \
			return;                                            \
		}                                                          \
	} while (0)

#define ASSERT_EQ(a, b)                                                    \
	do {                                                               \
		EXPECT_EQ(a, b);                                           \
		if (tf_failed) {                                           \
			tf_abort = true;                                   \
			return;                                            \
		}                                                          \
	} while (0)

#define FAIL(msg)                                                          \
	do {                                                               \
		TF_PRINT_LOC();                                            \
		std::printf("      FAIL: %s\n", msg);                      \
		tf_failed = true;                                          \
	} while (0)
