# Contributing to obs-ai-caption

Thanks for your interest! This plugin is early-stage; all contributions are
welcome — bug reports, feature requests, and pull requests alike.

## Development setup

1. Install a recent toolchain:
   - **macOS**: Xcode 15.1+ (or newer), CMake 3.28+, Ninja (optional)
   - **Windows**: Visual Studio 2022 with "Desktop development with C++" and
     Windows 10 SDK 10.0.20348+, CMake 3.28+
2. Clone this repo with submodules.
3. Run the tests first (no OBS required):
   ```bash
   cmake -S tests -B build-tests && cmake --build build-tests -j
   ctest --test-dir build-tests --output-on-failure
   ```

## Project layout

```
src/            # plugin sources
  plugin-main.cpp         # OBS entry point
  caption-filter.cpp      # filter implementation
  asr-engine.cpp          # sherpa-onnx wrapper + decode thread
  audio-ring-buffer.cpp   # SPSC ring buffer
  audio-analyzer.cpp      # RMS + F0 tracking
  audio-delay-buffer.cpp  # delay line + beep generator
  mute-word-list.cpp      # sensitive word matcher
  model-downloader.cpp    # Qt dialog to fetch sherpa-onnx models
  model-finder.cpp        # scans a model dir for encoder/decoder/joiner
  subtitle-manager.cpp    # partial/final caption buffering
tests/          # offline unit tests (GoogleTest-style lightweight framework)
cmake/          # build helpers, imported from obs-plugintemplate
buildspec.json  # declares prebuilt OBS / Qt6 / sherpa-onnx versions
```

## Coding standards

- C++17, tabs for indentation (see `.clang-format`)
- Avoid adding new top-level dependencies without discussion
- Keep files under 800 lines; extract modules aggressively
- All new behavior needs a unit test if it lives outside OBS plumbing

## Commit messages

Conventional-Commits style:

```
feat(filter): add cuda provider selector
fix(downloader): retry on transient 5xx
docs(readme): add Linux build hints
```

## Submitting a pull request

1. Fork and branch from `main`.
2. Run `clang-format` (`build-aux/run-clang-format/`) before pushing.
3. Push and open a PR against `main`. GitHub Actions will build for Windows
   and macOS on every push.
4. If the change is user-visible, update `CHANGELOG.md` under `[Unreleased]`.

## Release process

Maintainer-only:

1. Update `buildspec.json` `version` field.
2. Update `CHANGELOG.md`: promote `[Unreleased]` to a dated section.
3. Tag: `git tag -s v<X.Y.Z> -m "Release <X.Y.Z>"` and push.
4. The `dispatch.yaml` workflow builds, packages, and uploads artifacts to the
   corresponding GitHub Release.

## License

By contributing you agree to license your work under the same MIT
as the project.
