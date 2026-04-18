#pragma once

#include "asr-engine.h"

#include <string>

/* Scan `dir` for a transducer model triple (encoder/decoder/joiner) plus
 * tokens.txt (and optional bpe.vocab), populating the corresponding fields
 * of `config`. Returns true only when all three model files and tokens.txt
 * were located.
 *
 * Supports the common sherpa-onnx Zipformer2 streaming naming conventions
 * and will fall back to shorter generic names (encoder.onnx etc). */
bool find_model_files(const std::string &dir, AsrConfig &config);
