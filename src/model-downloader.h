#pragma once

#include <cstddef>
#include <string>

/* Model presets and on-demand downloader.
 *
 * Header is deliberately Qt-free so non-UI translation units
 * (caption-filter.cpp, asr-engine.cpp) can include it without pulling Qt.
 * The .cpp uses Qt for the actual dialog.
 */

struct ModelPreset {
	const char *id;               /* stable slug for the on-disk directory */
	const char *display_name;     /* human-readable menu label */
	const char *description;      /* one-line help */
	const char *archive_url;      /* tar.bz2 direct download */
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
 * Must be called from the UI thread. The `parent` argument is forwarded as a
 * QWidget*; we take it as void* so including this header does not require Qt. */
bool show_model_download_dialog(void *parent_widget, std::string *out_model_dir);
