#include "asr-engine.h"
#include "model-finder.h"
#include "test-framework.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

/* Create a temporary directory with optional files inside. Returns the
 * directory path. On failure an empty string is returned. */
std::string make_temp_dir()
{
#ifdef _WIN32
	char buf[L_tmpnam];
	std::tmpnam(buf);
	std::string dir = buf;
	std::string cmd = std::string("mkdir \"") + dir + "\"";
#else
	std::string dir = "/tmp/model-finder-test-";
	dir += std::to_string(static_cast<unsigned long long>(std::rand()));
	std::string cmd = std::string("mkdir -p \"") + dir + "\"";
#endif
	if (std::system(cmd.c_str()) != 0)
		return {};
	return dir;
}

void touch(const std::string &dir, const std::string &name)
{
	std::string sep =
#ifdef _WIN32
		"\\";
#else
		"/";
#endif
	std::ofstream out(dir + sep + name);
	out << "x";
}

void rm_tree(const std::string &dir)
{
#ifdef _WIN32
	std::string cmd = std::string("rmdir /s /q \"") + dir + "\"";
#else
	std::string cmd = std::string("rm -rf \"") + dir + "\"";
#endif
	(void)std::system(cmd.c_str());
}

} /* namespace */

TEST(ModelFinder, EmptyDirReturnsFalse)
{
	AsrConfig cfg;
	EXPECT_FALSE(find_model_files("", cfg));
}

TEST(ModelFinder, MissingModelFilesReturnsFalse)
{
	std::string dir = make_temp_dir();
	if (dir.empty()) {
		FAIL("cannot create temp dir");
		return;
	}
	touch(dir, "tokens.txt");
	/* No encoder/decoder/joiner */
	AsrConfig cfg;
	EXPECT_FALSE(find_model_files(dir, cfg));
	rm_tree(dir);
}

TEST(ModelFinder, FindsGenericNames)
{
	std::string dir = make_temp_dir();
	if (dir.empty()) {
		FAIL("cannot create temp dir");
		return;
	}
	touch(dir, "encoder.onnx");
	touch(dir, "decoder.onnx");
	touch(dir, "joiner.onnx");
	touch(dir, "tokens.txt");

	AsrConfig cfg;
	ASSERT_TRUE(find_model_files(dir, cfg));
	EXPECT_NE(cfg.encoder_path.find("encoder.onnx"), std::string::npos);
	EXPECT_NE(cfg.decoder_path.find("decoder.onnx"), std::string::npos);
	EXPECT_NE(cfg.joiner_path.find("joiner.onnx"), std::string::npos);
	EXPECT_NE(cfg.tokens_path.find("tokens.txt"), std::string::npos);
	EXPECT_TRUE(cfg.bpe_vocab_path.empty()); /* not present */
	rm_tree(dir);
}

TEST(ModelFinder, PrefersZipformer2NamesOverGeneric)
{
	std::string dir = make_temp_dir();
	if (dir.empty()) {
		FAIL("cannot create temp dir");
		return;
	}
	touch(dir, "encoder-epoch-99-avg-1-chunk-16-left-128.onnx");
	touch(dir, "encoder.onnx");
	touch(dir, "decoder-epoch-99-avg-1-chunk-16-left-128.onnx");
	touch(dir, "decoder.onnx");
	touch(dir, "joiner-epoch-99-avg-1-chunk-16-left-128.onnx");
	touch(dir, "joiner.onnx");
	touch(dir, "tokens.txt");

	AsrConfig cfg;
	ASSERT_TRUE(find_model_files(dir, cfg));
	EXPECT_NE(cfg.encoder_path.find("chunk-16-left-128"), std::string::npos);
	EXPECT_NE(cfg.decoder_path.find("chunk-16-left-128"), std::string::npos);
	EXPECT_NE(cfg.joiner_path.find("chunk-16-left-128"), std::string::npos);
	rm_tree(dir);
}

TEST(ModelFinder, PicksUpBpeVocabWhenPresent)
{
	std::string dir = make_temp_dir();
	if (dir.empty()) {
		FAIL("cannot create temp dir");
		return;
	}
	touch(dir, "encoder.onnx");
	touch(dir, "decoder.onnx");
	touch(dir, "joiner.onnx");
	touch(dir, "tokens.txt");
	touch(dir, "bpe.vocab");

	AsrConfig cfg;
	ASSERT_TRUE(find_model_files(dir, cfg));
	EXPECT_NE(cfg.bpe_vocab_path.find("bpe.vocab"), std::string::npos);
	rm_tree(dir);
}
