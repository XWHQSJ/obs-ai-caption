# AI Captions — on-device streaming ASR captions for OBS

> Free, offline, bilingual (English + 中文) AI captions with sensitive-word mute.

[GitHub](https://github.com/XWHQSJ/obs-ai-caption) · [Latest release](https://github.com/XWHQSJ/obs-ai-caption/releases/latest) · [Report issue](https://github.com/XWHQSJ/obs-ai-caption/issues) · MIT License

## Why this plugin?

| Traditional caption plugins | **AI Captions** |
| --- | --- |
| Send audio to Google / Azure / AWS | **100% on-device** streaming ASR |
| Pay per minute of transcription | **Free forever** — MIT open-source |
| Require a stable internet connection | Runs offline once the model is downloaded |
| English-only, often | **Bilingual 中文 + English** out of the box |
| No built-in profanity control | **Adaptive beep mute** that tracks the speaker's pitch |

Built on [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)'s streaming Zipformer transducer — the engine Next-gen Kaldi uses for low-latency production transcription.

## Features

- 🎯 **Real-time captions** — partial results within ~100 ms of speech, writes atomically to a text file any Text (GDI+ / FreeType 2) source can read.
- 📥 **One-click model download** — pick English / bilingual / tiny preset in the filter properties; the plugin downloads and extracts on demand.
- 🤫 **Sensitive-word mute** — load a hotwords file (`word :boost`); the plugin delays output audio so it can retroactively beep out matches. Beep frequency and volume adapt to the speaker's F0 + RMS.
- ⚡ **Hardware providers** — CPU (default), CUDA (Windows + NVIDIA), DirectML (Windows + any GPU). CoreML coming in v0.2.
- 🔐 **Supply-chain verifiable** — every release ships with a Sigstore build provenance attestation, so you can verify the binary came out of our public CI.

## Supported platforms

- Windows 10 / 11 x64
- macOS 11+ universal (Apple Silicon + Intel)

Linux users: the code compiles cleanly, we just aren't shipping builds yet — contributions welcome.

## Installation

Download from the [GitHub release](https://github.com/XWHQSJ/obs-ai-caption/releases/latest):

- **Windows**: extract the ZIP, merge `obs-plugins\` and `data\obs-plugins\` into `%ProgramFiles%\obs-studio\`. SmartScreen will prompt on first run → *More info* → *Run anyway*.
- **macOS**: open the `.pkg`. Gatekeeper will warn on this unsigned release; right-click → *Open* → *Open*, or run:
  ```bash
  curl -L https://github.com/XWHQSJ/obs-ai-caption/raw/main/scripts/unquarantine-macos.sh | bash
  ```

Verify the binary came out of our public CI run (optional):

```bash
gh attestation verify obs-ai-caption-0.1.0-macos-universal.pkg \
  --repo XWHQSJ/obs-ai-caption
```

## First use (60 seconds)

1. Right-click an audio source → **Filters** → **+** → **AI Captions**.
2. Click **Download Model…** and pick a preset.
3. Set **Caption Output File** to somewhere like `/tmp/captions.txt`.
4. Add a `Text (GDI+)` / `Text (FreeType 2)` source → enable **Read from file** → point at the same path.
5. Speak. Watch captions.

## Model presets

| Preset | Languages | Size | Latency |
| --- | --- | --- | --- |
| English (20M, fast) | en | ~70 MB | ~120 ms |
| Chinese + English (bilingual) | zh, en | ~300 MB | ~180 ms |
| English (tiny) | en | ~40 MB | ~90 ms |

## Links & credits

- **Source / issues**: https://github.com/XWHQSJ/obs-ai-caption
- **License**: MIT
- Built on:
  - [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) — Next-gen Kaldi team
  - [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
  - [OBS Studio](https://github.com/obsproject/obs-studio)

If this plugin saves you money on captioning, please star the repo and drop a thank-you to the sherpa-onnx team.
