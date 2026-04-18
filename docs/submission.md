# Submission kits for AI Captions (obs-ai-caption)

Drafts of everything you need to get the plugin listed on the OBS Studio
resources forum and, later, the official Plugin Registry.

## 1. OBS Forum resource page

When you create the resource at
<https://obsproject.com/forum/resources/categories/obs-studio-plugins.6/>,
paste the following as the long description. Fill in the angle-bracket
placeholders before posting.

```
# AI Captions for OBS

On-device, real-time AI captions powered by sherpa-onnx streaming ASR.
No cloud, no API keys. Includes a sensitive-word mute that replaces the
offending audio with an adaptive beep.

## Features

- [x] Streaming transcription (CPU / CUDA / DirectML)
- [x] English + Chinese/English bilingual models
- [x] One-click model download from the filter properties panel
- [x] Word-list mute with adaptive beep
- [x] Caption file output you can feed to any Text source

## Supported platforms

- Windows 10 / 11 x64
- macOS 11+ universal (Apple Silicon + Intel)

## Installation

Download the archive for your platform from the GitHub release and follow
the instructions in the README. The plugin is not code-signed yet — Windows
SmartScreen and macOS Gatekeeper will prompt on first launch; right-click
open to bypass.

## Links

- GitHub: <https://github.com/XWHQSJ/obs-ai-caption>
- Issues: <https://github.com/XWHQSJ/obs-ai-caption/issues>
- License: MIT

## Credits

Built on top of:

- [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) — Next-gen Kaldi team
- [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
```

Attach, in order:

1. Hero screenshot (filter properties panel with captions flowing)
2. Demo GIF / MP4 (5-10 s of live captions on top of a scene)
3. Model-download dialog screenshot
4. Sensitive-word mute demo (audio waveform before/after)

## 2. OBS Plugin Registry (OBS 31+)

The registry accepts a `plugin.yaml` manifest per release. Save the following
at the repo root as `plugin.yaml` before submitting:

```yaml
id: obs-ai-caption
displayName: AI Captions
authors:
  - name: obs-ai-caption contributors
    url: https://github.com/XWHQSJ
license: MIT
description: >
  Real-time streaming ASR captions powered by sherpa-onnx, with on-device
  inference and a built-in sensitive-word mute filter.
website: https://github.com/XWHQSJ/obs-ai-caption
issues: https://github.com/XWHQSJ/obs-ai-caption/issues
sourceUrl: https://github.com/XWHQSJ/obs-ai-caption
obsVersionRange:
  min: "31.0.0"
platforms:
  - windows-x64
  - macos-universal
```

Submission steps:

1. Open a PR against
   <https://github.com/obsproject/obs-plugin-registry> adding a directory
   `plugins/obs-ai-caption/<version>/` with the `plugin.yaml` above and the
   release artifacts you want the registry to distribute (or just
   `downloadUrl`s if you host them yourself on GitHub Releases).
2. A maintainer will review. Expect feedback on:
   - Signed binaries (they will ask even though the registry accepts
     unsigned plugins with warnings)
   - License compatibility (MIT — compatible with OBS GPL; sherpa-onnx is Apache-2.0)
   - Model download UX (we already show the source + progress bar)
3. On merge your plugin becomes available in OBS **Plugin Manager** inside
   OBS Studio 31+.

## 3. Announcement channels

After the forum + registry listings go live:

- [ ] Post a release note on r/obs
- [ ] Tweet / Mastodon with the hero screenshot
- [ ] Open an issue in the sherpa-onnx repo under "made-with" asking to be
      listed
- [ ] Add to awesome-obs: <https://github.com/awesome-foss/awesome-obs>
