# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- GitHub Actions CI for Windows x64 and macOS universal
- On-demand ASR model downloader UI with 3 preset models
- Project restructured to match official `obs-plugintemplate`
- `buildspec.json` drives OBS / Qt6 / sherpa-onnx dependency fetch

### Changed
- Plugin now loads via `obs_register_source` from C++ `plugin-main.cpp`
- Settings UI gains a **Download Model...** button

### Fixed
- Consumer-side lost-wakeup race in `AudioRingBuffer::wait_for_data`
- Caption file never being written because of duplicate
  `should_emit_file_update` consumption
- ASR stats log de-duplicating against stale counters after engine restart

## [0.1.0] - 2026-04-18

Initial development release — Windows-only; not distributed publicly.
