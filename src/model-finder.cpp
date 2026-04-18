#include "model-finder.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

bool file_exists(const std::string &path)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		return false;
	std::fclose(f);
	return true;
}

std::string path_join(const std::string &dir, const std::string &file)
{
#ifdef _WIN32
	const char sep = '\\';
#else
	const char sep = '/';
#endif
	if (dir.empty())
		return file;
	if (dir.back() == '/' || dir.back() == '\\')
		return dir + file;
	return dir + sep + file;
}

/* First hit wins. Order: longest-filenames first, then generic fallbacks. */
const std::vector<std::string> kEncoderPatterns{
	"encoder-epoch-99-avg-1-chunk-16-left-128.onnx",
	"encoder-epoch-99-avg-1-chunk-16-left-128.int8.onnx",
	"encoder-epoch-99-avg-1-chunk-16-left-64.onnx",
	"encoder-epoch-99-avg-1-chunk-16-left-64.int8.onnx",
	"encoder-epoch-99-avg-1.onnx",
	"encoder-epoch-99-avg-1.int8.onnx",
	"encoder.onnx",
	"encoder.int8.onnx",
};

const std::vector<std::string> kDecoderPatterns{
	"decoder-epoch-99-avg-1-chunk-16-left-128.onnx",
	"decoder-epoch-99-avg-1-chunk-16-left-64.onnx",
	"decoder-epoch-99-avg-1.onnx",
	"decoder.onnx",
};

const std::vector<std::string> kJoinerPatterns{
	"joiner-epoch-99-avg-1-chunk-16-left-128.onnx",
	"joiner-epoch-99-avg-1-chunk-16-left-128.int8.onnx",
	"joiner-epoch-99-avg-1-chunk-16-left-64.onnx",
	"joiner-epoch-99-avg-1-chunk-16-left-64.int8.onnx",
	"joiner-epoch-99-avg-1.onnx",
	"joiner-epoch-99-avg-1.int8.onnx",
	"joiner.onnx",
	"joiner.int8.onnx",
};

bool find_first(const std::string &dir, const std::vector<std::string> &patterns,
		std::string &out)
{
	for (const auto &p : patterns) {
		std::string path = path_join(dir, p);
		if (file_exists(path)) {
			out = std::move(path);
			return true;
		}
	}
	return false;
}

} /* namespace */

bool find_model_files(const std::string &dir, AsrConfig &config)
{
	if (dir.empty())
		return false;

	std::string enc, dec, join;
	const bool found_enc = find_first(dir, kEncoderPatterns, enc);
	const bool found_dec = find_first(dir, kDecoderPatterns, dec);
	const bool found_join = find_first(dir, kJoinerPatterns, join);

	const std::string tokens = path_join(dir, "tokens.txt");
	if (!file_exists(tokens))
		return false;

	config.tokens_path = tokens;

	const std::string bpe_vocab = path_join(dir, "bpe.vocab");
	if (file_exists(bpe_vocab))
		config.bpe_vocab_path = bpe_vocab;

	if (!(found_enc && found_dec && found_join))
		return false;

	config.encoder_path = std::move(enc);
	config.decoder_path = std::move(dec);
	config.joiner_path = std::move(join);
	return true;
}
