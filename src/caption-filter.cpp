#include "caption-filter.h"
#include "asr-engine.h"
#include "audio-analyzer.h"
#include "audio-delay-buffer.h"
#include "constants.h"
#include "model-downloader.h"
#include "model-finder.h"
#include "mute-word-list.h"
#include "subtitle-manager.h"

#include <obs-module.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ai_caption::constants;

/* -------------------------------------------------------- */

#define do_log(level, format, ...) \
	blog(level, "[AI Caption: '%s'] " format, obs_source_get_name(cf->context), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

/* -------------------------------------------------------- */
/* Property keys                                            */

#define S_MODEL_DIR       "model_dir"
#define S_NUM_THREADS     "num_threads"
#define S_PROVIDER        "provider"
#define S_OUTPUT_FILE     "output_file"
#define S_MAX_LINES       "max_lines"
#define S_CLEAR_TIMEOUT   "clear_timeout"
#define S_ENABLE_ENDPOINT "enable_endpoint"
#define S_RULE2_SILENCE   "rule2_silence"
#define S_ENABLE_MUTE     "enable_mute"
#define S_MUTE_WORDS_FILE "mute_words_file"
#define S_MUTE_DELAY_MS   "mute_delay_ms"
#define S_MUTE_PADDING_MS "mute_padding_ms"

#define MT_ obs_module_text

/* -------------------------------------------------------- */
/* Filter data                                              */

struct MuteInterval {
	uint64_t start_frame;
	uint64_t end_frame;
};

struct caption_filter_data {
	obs_source_t *context;
	AsrEngine engine;

	std::thread init_thread;
	std::mutex init_mutex;
	std::atomic<bool> init_in_progress{false};

	/* ---- Caption output ---- */
	std::mutex text_mutex;
	SubtitleManager subtitles;
	std::string output_file_path; /* guarded by settings_mutex */

	std::vector<float> mono_buf;            /* OBS audio thread only */
	std::vector<uint8_t> chunk_mute_flags;  /* OBS audio thread only */

	/* ---- Audio timing ---- */
	std::atomic<uint64_t> total_audio_frames{0};
	std::atomic<uint32_t> current_sample_rate{kDefaultSampleRate};
	std::atomic<size_t> current_channels{2};
	int last_bad_word_count = 0;            /* ASR thread only */

	/* ---- Mute state ---- */
	std::atomic<bool> mute_enabled{false};
	std::atomic<uint32_t> mute_delay_ms{500};
	std::atomic<uint32_t> mute_padding_ms{300};

	MuteWordList mute_words;
	AudioAnalyzer analyzer;                 /* OBS audio thread only */
	AudioDelayBuffer delay_buf;             /* OBS audio thread only */
	BeepGenerator beep;                     /* OBS audio thread only */
	bool had_beep_last_chunk = false;       /* OBS audio thread only */

	std::mutex mute_intervals_mutex;
	std::vector<MuteInterval> mute_intervals;

	/* ---- Settings snapshot (guarded by settings_mutex) ---- */
	std::mutex settings_mutex;
	std::string model_dir;
	int num_threads = 2;
	std::string provider = "cpu";
	bool enable_endpoint = true;
	float rule2_silence = 1.2f;
	std::string mute_words_file;

	/* ---- ASR stats logging (ASR thread only) ---- */
	AsrStats last_logged_asr_stats{};
	uint64_t last_asr_stats_log_ms = 0;
};

/* -------------------------------------------------------- */

static uint64_t now_ms()
{
	using namespace std::chrono;
	return static_cast<uint64_t>(
		duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

/* -------------------------------------------------------- */
/* Caption file output                                      */

/* Write the current subtitle state to disk when an update is pending.
 * Assumes the caller has already verified via SubtitleManager::should_emit_file_update
 * that a write is needed; this function just composes the content and does
 * the IO. */
static void write_caption_file(caption_filter_data *cf, const std::string &content)
{
	std::string output_path;
	{
		std::lock_guard<std::mutex> lk(cf->settings_mutex);
		output_path = cf->output_file_path;
	}
	if (output_path.empty())
		return;

	const std::string tmp_path = output_path + ".tmp";
	{
		std::ofstream out(tmp_path, std::ios::trunc);
		if (!out.is_open()) {
			warn("Failed to open caption temp file '%s'", tmp_path.c_str());
			return;
		}
		out << content;
	}
#ifdef _WIN32
	/* rename() on Windows fails if destination exists; remove first. */
	if (std::remove(output_path.c_str()) != 0 && errno != ENOENT)
		warn("Failed to remove caption file '%s': %s", output_path.c_str(),
		     std::strerror(errno));
#endif
	if (std::rename(tmp_path.c_str(), output_path.c_str()) != 0)
		warn("Failed to rename caption file '%s' -> '%s': %s",
		     tmp_path.c_str(), output_path.c_str(), std::strerror(errno));
}

/* Checks whether a write is needed and, if so, composes the content and
 * writes. Combines the "should emit" check with "compose" under text_mutex
 * so that both see a consistent snapshot. */
static void maybe_write_caption_file(caption_filter_data *cf)
{
	std::string content;
	{
		std::lock_guard<std::mutex> lk(cf->text_mutex);
		if (!cf->subtitles.should_emit_file_update(now_ms()))
			return;
		content = cf->subtitles.compose_output(now_ms());
	}
	write_caption_file(cf, content);
}

/* -------------------------------------------------------- */
/* ASR result callback                                      */

static void maybe_log_asr_stats(caption_filter_data *cf)
{
	const uint64_t current_time_ms = now_ms();
	if (current_time_ms < cf->last_asr_stats_log_ms + kAsrStatsLogIntervalMs)
		return;

	const AsrStats stats = cf->engine.get_stats();
	if (stats.dropped_samples == cf->last_logged_asr_stats.dropped_samples &&
	    stats.decode_errors == cf->last_logged_asr_stats.decode_errors)
		return;

	cf->last_logged_asr_stats = stats;
	cf->last_asr_stats_log_ms = current_time_ms;
	warn("ASR stats changed: dropped_samples=%llu decode_errors=%llu",
	     static_cast<unsigned long long>(stats.dropped_samples),
	     static_cast<unsigned long long>(stats.decode_errors));
}

static void check_mute_trigger(caption_filter_data *cf, const std::string &text, bool is_partial,
			       float token_age_sec)
{
	if (!cf->mute_enabled.load(std::memory_order_acquire))
		return;

	const int current_bad_words = cf->mute_words.count_matches(text);
	if (current_bad_words > cf->last_bad_word_count) {
		const uint64_t current_frame = cf->total_audio_frames.load(std::memory_order_acquire);
		const size_t ring_size = cf->engine.get_pending_samples();
		const uint64_t decoded_frame =
			(current_frame > ring_size) ? current_frame - ring_size : 0;

		uint32_t sample_rate = cf->current_sample_rate.load(std::memory_order_acquire);
		if (sample_rate == 0)
			sample_rate = kDefaultSampleRate;

		/* If timestamps aren't available, token_age_sec is ~0. In that case,
		 * fall back to assuming a conservative ASR latency. Otherwise use
		 * the reported age + padding for the duration of the word. */
		float lookback_sec = kDefaultAsrLatencySec;
		if (token_age_sec > 0.01f)
			lookback_sec = token_age_sec + kMuteLookbackPaddingSec;

		const uint64_t lookback_frames =
			static_cast<uint64_t>(sample_rate * lookback_sec);
		const uint32_t padding_ms = cf->mute_padding_ms.load(std::memory_order_acquire);
		const uint64_t padding_frames =
			static_cast<uint64_t>(sample_rate) * padding_ms / 1000;

		const uint64_t start_mute_frame =
			(decoded_frame > lookback_frames) ? decoded_frame - lookback_frames : 0;
		const uint64_t end_mute_frame = start_mute_frame + lookback_frames + padding_frames;

		{
			std::lock_guard<std::mutex> lk(cf->mute_intervals_mutex);
			cf->mute_intervals.push_back({start_mute_frame, end_mute_frame});
		}

		info("MUTE TRIGGERED! Bad words %d -> %d. Frames [%llu..%llu] "
		     "(decoded=%llu ring=%zu token_age=%.2fs lookback=%.2fs)",
		     cf->last_bad_word_count, current_bad_words,
		     static_cast<unsigned long long>(start_mute_frame),
		     static_cast<unsigned long long>(end_mute_frame),
		     static_cast<unsigned long long>(decoded_frame), ring_size,
		     static_cast<double>(token_age_sec), static_cast<double>(lookback_sec));

		cf->last_bad_word_count = current_bad_words;
	}
	/* If current_bad_words < last, ASR revised its output — keep the old
	 * count to prevent double-triggering on subsequent partials. */

	if (!is_partial)
		cf->last_bad_word_count = 0;
}

static void on_asr_result(caption_filter_data *cf, const std::string &text, bool is_partial,
			  float token_age_sec)
{
	check_mute_trigger(cf, text, is_partial, token_age_sec);

	bool should_write_file = false;
	{
		std::lock_guard<std::mutex> lk(cf->text_mutex);
		const uint64_t cur_ms = now_ms();
		if (is_partial)
			cf->subtitles.on_partial(text, cur_ms);
		else
			cf->subtitles.on_final(text, cur_ms);
		should_write_file = cf->subtitles.should_emit_file_update(cur_ms);
	}

	if (!is_partial)
		info("ASR FINAL: '%s'", text.c_str());

	if (should_write_file) {
		std::string content;
		{
			std::lock_guard<std::mutex> lk(cf->text_mutex);
			content = cf->subtitles.compose_output(now_ms());
		}
		write_caption_file(cf, content);
	}
}

/* -------------------------------------------------------- */
/* OBS callbacks                                            */

static const char *caption_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return MT_("AICaptions");
}

static void join_init_thread(caption_filter_data *cf)
{
	std::lock_guard<std::mutex> lock(cf->init_mutex);
	if (cf->init_thread.joinable())
		cf->init_thread.join();
}

static void caption_filter_destroy(void *data)
{
	auto *cf = static_cast<caption_filter_data *>(data);
	cf->engine.shutdown();
	join_init_thread(cf);
	delete cf;
}

static void start_asr_async(caption_filter_data *cf)
{
	join_init_thread(cf);

	AsrConfig snapshot;
	std::string model_dir;
	{
		std::lock_guard<std::mutex> lk(cf->settings_mutex);
		model_dir = cf->model_dir;
		snapshot.num_threads = cf->num_threads;
		snapshot.provider = cf->provider;
		snapshot.enable_endpoint = cf->enable_endpoint;
		snapshot.rule2_min_trailing_silence = cf->rule2_silence;
	}

	std::string hotwords_buf = cf->mute_words.get_hotwords_buf();
	const bool has_hotwords = !hotwords_buf.empty();

	cf->init_in_progress.store(true, std::memory_order_release);

	std::lock_guard<std::mutex> lock(cf->init_mutex);
	cf->init_thread = std::thread([cf, model_dir, snapshot, hotwords_buf, has_hotwords]() mutable {
		AsrConfig config = snapshot;

		if (!find_model_files(model_dir, config)) {
			blog(LOG_WARNING, "[AI Caption] Cannot find model files in: %s",
			     model_dir.c_str());
			cf->init_in_progress.store(false, std::memory_order_release);
			return;
		}

		if (has_hotwords) {
			config.decoding_method = "modified_beam_search";
			config.max_active_paths = kHotwordsMaxActivePaths;
			config.hotwords_buf = std::move(hotwords_buf);
			config.hotwords_score = kHotwordsScore;
		}

		cf->engine.set_result_callback(
			[cf](const std::string &text, bool is_partial, float token_age_sec) {
				on_asr_result(cf, text, is_partial, token_age_sec);
			});

		blog(LOG_INFO, "[AI Caption] Loading model from: %s (hotwords: %s)",
		     model_dir.c_str(), has_hotwords ? "enabled" : "disabled");

		if (!cf->engine.init(config)) {
			blog(LOG_WARNING, "[AI Caption] Failed to initialize ASR engine");
			cf->init_in_progress.store(false, std::memory_order_release);
			return;
		}

		cf->init_in_progress.store(false, std::memory_order_release);
		blog(LOG_INFO, "[AI Caption] ASR engine started (encoder=%s)",
		     config.encoder_path.c_str());
	});
}

static void caption_filter_update(void *data, obs_data_t *settings)
{
	auto *cf = static_cast<caption_filter_data *>(data);

	std::string new_model_dir = obs_data_get_string(settings, S_MODEL_DIR);
	int new_threads = static_cast<int>(obs_data_get_int(settings, S_NUM_THREADS));
	std::string new_provider = obs_data_get_string(settings, S_PROVIDER);
	bool new_enable_ep = obs_data_get_bool(settings, S_ENABLE_ENDPOINT);
	float new_rule2 = static_cast<float>(obs_data_get_double(settings, S_RULE2_SILENCE));
	std::string new_mute_file = obs_data_get_string(settings, S_MUTE_WORDS_FILE);
	std::string new_output_file = obs_data_get_string(settings, S_OUTPUT_FILE);
	int new_max_lines = static_cast<int>(obs_data_get_int(settings, S_MAX_LINES));
	double new_clear_timeout = obs_data_get_double(settings, S_CLEAR_TIMEOUT);

	/* Captions settings (independent of ASR engine restart) */
	{
		std::lock_guard<std::mutex> lk(cf->text_mutex);
		cf->subtitles.set_max_lines(new_max_lines);
		cf->subtitles.set_clear_timeout_sec(new_clear_timeout);
		cf->subtitles.set_partial_write_throttle_ms(kPartialWriteThrottleMs);
	}

	cf->mute_enabled.store(obs_data_get_bool(settings, S_ENABLE_MUTE),
			       std::memory_order_release);
	{
		const auto raw_delay =
			static_cast<uint32_t>(obs_data_get_int(settings, S_MUTE_DELAY_MS));
		const auto raw_pad =
			static_cast<uint32_t>(obs_data_get_int(settings, S_MUTE_PADDING_MS));
		cf->mute_delay_ms.store(std::min(raw_delay, kMaxMuteDelayMs),
					std::memory_order_release);
		cf->mute_padding_ms.store(std::min(raw_pad, kMaxMutePaddingMs),
					  std::memory_order_release);
	}

	bool need_restart = false;
	bool mute_file_changed = false;
	{
		std::lock_guard<std::mutex> lk(cf->settings_mutex);
		mute_file_changed = (new_mute_file != cf->mute_words_file);
		need_restart = (new_model_dir != cf->model_dir) ||
			       (new_threads != cf->num_threads) ||
			       (new_provider != cf->provider) ||
			       (new_enable_ep != cf->enable_endpoint) ||
			       (new_rule2 != cf->rule2_silence) ||
			       mute_file_changed;

		cf->model_dir = new_model_dir;
		cf->num_threads = new_threads;
		cf->provider = new_provider;
		cf->enable_endpoint = new_enable_ep;
		cf->rule2_silence = new_rule2;
		cf->output_file_path = new_output_file;
		if (mute_file_changed)
			cf->mute_words_file = new_mute_file;
	}

	if (mute_file_changed)
		cf->mute_words.load(new_mute_file);

	if (need_restart && !new_model_dir.empty()) {
		cf->engine.shutdown();
		/* Engine stats were reset by shutdown(); keep the local mirror in
		 * sync so maybe_log_asr_stats() doesn't misinterpret the fresh
		 * counters as "unchanged since last log". */
		cf->last_logged_asr_stats = AsrStats{};
		cf->last_asr_stats_log_ms = 0;
		start_asr_async(cf);
	}

	/* If mute was just disabled, stop carrying old intervals forward. */
	if (!cf->mute_enabled.load(std::memory_order_acquire)) {
		std::lock_guard<std::mutex> lk(cf->mute_intervals_mutex);
		cf->mute_intervals.clear();
	}
}

static void *caption_filter_create(obs_data_t *settings, obs_source_t *filter)
{
	auto *cf = new caption_filter_data();
	cf->context = filter;
	caption_filter_update(cf, settings);
	return cf;
}

/* -------------------------------------------------------- */
/* Audio filter                                             */

static struct obs_audio_data *caption_filter_audio(void *data, struct obs_audio_data *audio)
{
	auto *cf = static_cast<caption_filter_data *>(data);

	if (!cf->engine.is_running() || audio->frames == 0)
		return audio;

	const uint32_t sample_rate = audio_output_get_sample_rate(obs_get_audio());
	const size_t channels = audio_output_get_channels(obs_get_audio());

	cf->current_sample_rate.store(sample_rate, std::memory_order_relaxed);
	cf->current_channels.store(channels, std::memory_order_relaxed);
	const uint64_t current_frame_start =
		cf->total_audio_frames.load(std::memory_order_acquire);
	cf->total_audio_frames.fetch_add(audio->frames, std::memory_order_release);

	/* ---- Downmix to mono for ASR ---- */
	if (cf->mono_buf.size() < audio->frames)
		cf->mono_buf.resize(std::max<size_t>(4096, audio->frames));

	float *mono = cf->mono_buf.data();
	std::memset(mono, 0, audio->frames * sizeof(float));

	if (channels == 1 && audio->data[0]) {
		std::memcpy(mono, audio->data[0], audio->frames * sizeof(float));
	} else {
		for (size_t ch = 0; ch < channels; ch++) {
			if (!audio->data[ch])
				continue;
			auto *src = reinterpret_cast<float *>(audio->data[ch]);
			for (uint32_t i = 0; i < audio->frames; i++)
				mono[i] += src[i];
		}
		const float inv = 1.0f / static_cast<float>(channels);
		for (uint32_t i = 0; i < audio->frames; i++)
			mono[i] *= inv;
	}

	cf->engine.feed_audio(mono, audio->frames, static_cast<int>(sample_rate));

	/* ---- Adaptive beep muting ---- */
	const bool mute_enabled = cf->mute_enabled.load(std::memory_order_acquire);
	const uint32_t mute_delay_ms = cf->mute_delay_ms.load(std::memory_order_acquire);

	if (mute_enabled && mute_delay_ms > 0) {
		cf->analyzer.feed(mono, audio->frames, sample_rate);
		cf->delay_buf.configure(sample_rate, channels, mute_delay_ms);

		const size_t delay_frames = cf->delay_buf.get_delay_frames();
		const uint64_t out_frame_start =
			(current_frame_start > delay_frames) ? current_frame_start - delay_frames : 0;

		if (cf->chunk_mute_flags.size() < audio->frames)
			cf->chunk_mute_flags.resize(std::max<size_t>(4096, audio->frames));
		std::fill_n(cf->chunk_mute_flags.begin(), audio->frames, static_cast<uint8_t>(0));
		bool any_beep = false;

		{
			std::lock_guard<std::mutex> lk(cf->mute_intervals_mutex);
			auto it = cf->mute_intervals.begin();
			while (it != cf->mute_intervals.end()) {
				if (it->end_frame < out_frame_start)
					it = cf->mute_intervals.erase(it);
				else
					++it;
			}

			for (const auto &interval : cf->mute_intervals) {
				const uint64_t start_idx =
					(interval.start_frame > out_frame_start)
						? interval.start_frame - out_frame_start
						: 0;
				const uint64_t end_idx =
					(interval.end_frame >= out_frame_start)
						? interval.end_frame - out_frame_start
						: 0;

				if (start_idx < audio->frames) {
					const size_t fill_end = std::min<size_t>(
						audio->frames, static_cast<size_t>(end_idx) + 1);
					for (size_t i = start_idx; i < fill_end; i++) {
						cf->chunk_mute_flags[i] = 1;
						any_beep = true;
					}
				}
			}
		}

		if (any_beep) {
			const float f0 = cf->analyzer.get_freq();
			const float rms = cf->analyzer.get_rms();

			float beep_freq = f0 * kBeepFreqMultiplier;
			beep_freq = std::clamp(beep_freq, kBeepFreqMinHz, kBeepFreqMaxHz);

			float beep_vol = rms * kBeepVolumeMultiplier;
			beep_vol = std::clamp(beep_vol, kBeepVolumeMin, kBeepVolumeMax);

			if (!cf->had_beep_last_chunk) {
				info("Adaptive beep: F0=%.2fHz -> %.2fHz, RMS=%.4f -> %.2f",
				     static_cast<double>(f0), static_cast<double>(beep_freq),
				     static_cast<double>(rms), static_cast<double>(beep_vol));
			}
			cf->had_beep_last_chunk = true;
			cf->beep.set_params(sample_rate, beep_freq, beep_vol);
		} else {
			if (cf->had_beep_last_chunk)
				info("Beep replacement stopped.");
			cf->had_beep_last_chunk = false;
		}

		cf->delay_buf.process(reinterpret_cast<float **>(audio->data), audio->frames,
				      cf->chunk_mute_flags, cf->beep);
	}

	maybe_write_caption_file(cf);
	maybe_log_asr_stats(cf);
	return audio;
}

/* -------------------------------------------------------- */
/* Properties                                               */

static void caption_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_MODEL_DIR, "");
	obs_data_set_default_int(settings, S_NUM_THREADS, 2);
	obs_data_set_default_string(settings, S_PROVIDER, "cpu");
	obs_data_set_default_string(settings, S_OUTPUT_FILE, "");
	obs_data_set_default_int(settings, S_MAX_LINES, 2);
	obs_data_set_default_double(settings, S_CLEAR_TIMEOUT, 5.0);
	obs_data_set_default_bool(settings, S_ENABLE_ENDPOINT, true);
	obs_data_set_default_double(settings, S_RULE2_SILENCE, 1.2);
	obs_data_set_default_bool(settings, S_ENABLE_MUTE, false);
	obs_data_set_default_string(settings, S_MUTE_WORDS_FILE, "");
	obs_data_set_default_int(settings, S_MUTE_DELAY_MS, 500);
	obs_data_set_default_int(settings, S_MUTE_PADDING_MS, 300);
}

static obs_properties_t *caption_filter_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, S_MODEL_DIR, MT_("ModelDir"),
				OBS_PATH_DIRECTORY, nullptr, nullptr);

	/* Button that opens the model-download dialog. The callback writes the
	 * resolved model path back into S_MODEL_DIR so the filter picks it up
	 * via the normal `update` flow. */
	obs_properties_add_button2(
		props, "download_model_button", MT_("DownloadModel"),
		[](obs_properties_t * /*props*/, obs_property_t * /*prop*/,
		   void *button_data) -> bool {
			std::string new_dir;
			if (!show_model_download_dialog(nullptr, &new_dir))
				return false;
			auto *cf = static_cast<caption_filter_data *>(button_data);
			if (cf && cf->context) {
				OBSDataAutoRelease settings =
					obs_source_get_settings(cf->context);
				obs_data_set_string(settings, S_MODEL_DIR,
						    new_dir.c_str());
				obs_source_update(cf->context, settings);
			}
			return true;
		},
		data);
	obs_properties_add_int(props, S_NUM_THREADS, MT_("NumThreads"), 1, 8, 1);

	obs_property_t *provider = obs_properties_add_list(
		props, S_PROVIDER, MT_("Provider"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(provider, "CPU", "cpu");
	obs_property_list_add_string(provider, "CUDA (NVIDIA GPU)", "cuda");
	obs_property_list_add_string(provider, "DirectML (Any GPU)", "directml");

	obs_properties_add_path(props, S_OUTPUT_FILE, MT_("OutputFile"),
				OBS_PATH_FILE_SAVE, "Text (*.txt)", nullptr);
	obs_properties_add_int(props, S_MAX_LINES, MT_("MaxLines"), 1, 5, 1);
	obs_property_t *timeout = obs_properties_add_float_slider(
		props, S_CLEAR_TIMEOUT, MT_("ClearTimeout"), 1.0, 30.0, 0.5);
	obs_property_float_set_suffix(timeout, " s");

	obs_properties_add_bool(props, S_ENABLE_ENDPOINT, MT_("EnableEndpoint"));
	obs_property_t *silence = obs_properties_add_float_slider(
		props, S_RULE2_SILENCE, MT_("EndpointSilence"), 0.2, 5.0, 0.1);
	obs_property_float_set_suffix(silence, " s");

	obs_properties_add_bool(props, S_ENABLE_MUTE, MT_("EnableMute"));
	obs_properties_add_path(props, S_MUTE_WORDS_FILE, MT_("MuteWordsFile"),
				OBS_PATH_FILE, "Text (*.txt)", nullptr);
	obs_property_t *delay = obs_properties_add_int_slider(
		props, S_MUTE_DELAY_MS, MT_("MuteDelay"), 200, 2000, 50);
	obs_property_int_set_suffix(delay, " ms");
	obs_property_t *padding = obs_properties_add_int_slider(
		props, S_MUTE_PADDING_MS, MT_("MutePadding"), 100, 1000, 50);
	obs_property_int_set_suffix(padding, " ms");

	return props;
}

/* -------------------------------------------------------- */

struct obs_source_info ai_caption_filter_info = {};

static void init_filter_info()
{
	ai_caption_filter_info.id = "ai_caption_filter";
	ai_caption_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	ai_caption_filter_info.output_flags = OBS_SOURCE_AUDIO;
	ai_caption_filter_info.get_name = caption_filter_name;
	ai_caption_filter_info.create = caption_filter_create;
	ai_caption_filter_info.destroy = caption_filter_destroy;
	ai_caption_filter_info.update = caption_filter_update;
	ai_caption_filter_info.filter_audio = caption_filter_audio;
	ai_caption_filter_info.get_defaults = caption_filter_defaults;
	ai_caption_filter_info.get_properties = caption_filter_properties;
}

struct FilterInfoInit {
	FilterInfoInit() { init_filter_info(); }
};
static FilterInfoInit _filter_info_init;
