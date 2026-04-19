# SignPath Foundation — free Windows code signing setup

SignPath Foundation provides free Authenticode + EV code signing for
qualifying OSS projects. Obtaining a slot is a one-time manual step;
the CI integration is already stubbed out on our side.

## Apply

1. Go to <https://signpath.org/apply>.
2. Fill the form with the following canonical values:

   | Field | Value |
   | --- | --- |
   | Project Name | `obs-ai-caption` |
   | Repository URL | `https://github.com/XWHQSJ/obs-ai-caption` |
   | Homepage URL | `https://github.com/XWHQSJ/obs-ai-caption` |
   | Download URL | `https://github.com/XWHQSJ/obs-ai-caption/releases/latest` (mentions SignPath Foundation ✓) |
   | Privacy Policy URL | *leave blank — plugin collects no data* |
   | Wikipedia URL | *blank* |
   | Tagline | `Free, on-device streaming AI captions for OBS Studio` |
   | Description | `Real-time streaming ASR captions for OBS Studio powered by sherpa-onnx, with an adaptive-beep sensitive-word mute filter. Runs fully on-device; no cloud, no API keys. MIT licensed, bilingual Chinese + English.` |
   | Maintainer Type | `Individual maintainer` |
   | Build System | `GitHub Actions` |
   | First Name / Last Name / Email | your legal name + project email |
   | Company | *blank for individuals* |
   | Primary Discovery Channel | `Search engine` (or however you found us) |

3. Submit. Expect a reply from `rene@signpath.org` within 2–4 weeks.
4. You'll then receive onboarding emails with:
   - Your SignPath organization ID
   - A signing-policy slug (we suggest `release-signing`)
   - An API token for GitHub Actions

## Plug it into our CI

We already have the integration skeleton commented out in
`.github/workflows/push.yaml`. Once approved, you just need to:

1. Add **repository secrets**:
   - `SIGNPATH_API_TOKEN`
2. Add **repository variables**:
   - `SIGNPATH_ORG_ID` — the UUID returned during onboarding
3. Uncomment the `sign-windows:` block we pre-wrote, push to main, tag a new
   version, and the Windows ZIP will be signed automatically.

## Add a “we sign with SignPath Foundation” badge to the README

Once your project is listed on <https://signpath.org/projects>, we can
swap the plain "unsigned" notice in README for:

```md
[![Signed by SignPath Foundation](https://signpath.org/assets/favicon.ico) Windows builds signed by SignPath Foundation for open source](https://signpath.org/projects/obs-ai-caption)
```

Following the SignPath Foundation brand guidelines in their onboarding docs.
