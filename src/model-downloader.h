#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <string>

/* Model presets and on-demand downloader.
 *
 * The plugin ships without any ASR model. On first use (or when the user clicks
 * "Download model" in the filter settings), the downloader fetches a selected
 * sherpa-onnx streaming Zipformer model into the OBS plugin-config directory
 * and extracts it so `find_model_files()` can discover it.
 *
 * Directory layout after a successful download:
 *   <OBS_CONFIG>/plugin_config/obs-ai-caption/models/
 *     <preset-id>/
 *       encoder-*.onnx
 *       decoder-*.onnx
 *       joiner-*.onnx
 *       tokens.txt
 *       ...
 */

struct ModelPreset {
	const char *id;          /* stable slug for the on-disk directory */
	const char *display_name; /* human-readable menu label */
	const char *description;  /* one-line help */
	const char *archive_url;  /* tar.bz2 direct download */
	const char *extracted_subdir; /* subdir inside archive (strip one level) */
};

/* Returns a static list of known presets, in display order. */
const ModelPreset *model_presets(size_t *out_count);

/* Returns absolute path to the OBS plugin-config models root, creating it
 * if needed. */
std::string models_root_dir();

/* Returns the on-disk directory for a given preset, whether or not it exists. */
std::string preset_install_dir(const ModelPreset &preset);

/* Returns true if `dir` looks like a usable sherpa-onnx model dir. */
bool preset_is_installed(const ModelPreset &preset);

/* Opens a Qt dialog that lets the user pick a model preset and downloads it
 * to `preset_install_dir`. Returns true and fills `out_model_dir` on success.
 * Must be called from the UI thread. */
bool show_model_download_dialog(QWidget *parent, std::string *out_model_dir);
