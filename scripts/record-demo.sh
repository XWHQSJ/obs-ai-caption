#!/usr/bin/env bash
# Records an OBS session into docs/screenshots/hero.gif.
#
# Prerequisites:
#   brew install ffmpeg gifski
#
# Usage:
#   scripts/record-demo.sh [seconds]
#
# What it does:
#   1. Prompts you to arrange the OBS window the way you want captured
#   2. Records the full screen for N seconds (default 12) at 15 fps via ffmpeg AVFoundation
#   3. Converts the mp4 to an optimized GIF via gifski
#   4. Writes docs/screenshots/hero.gif
#
# Tip: on macOS you'll be asked to grant Screen Recording permission to
# Terminal the first time; approve it in System Settings → Privacy & Security.
set -euo pipefail

DURATION=${1:-12}
OUTDIR=docs/screenshots
mkdir -p "$OUTDIR"

TMP_MP4=$(mktemp -t obs-demo).mp4
TMP_FRAMES=$(mktemp -d -t obs-frames)
trap 'rm -f "$TMP_MP4"; rm -rf "$TMP_FRAMES"' EXIT

# Discover the primary display's AVFoundation device index. ffmpeg prints
# lines like "[AVFoundation indev @ ...] [2] Capture screen 0" — pull the
# number inside the FIRST pair of brackets on such a line.
# NB: head -1 triggers SIGPIPE on upstream sed which errexit+pipefail would
# treat as fatal, so disable those just for this pipeline.
set +o pipefail
DEVICE_INDEX=$(ffmpeg -f avfoundation -list_devices true -i "" 2>&1 \
  | grep -E "\[[0-9]+\] Capture screen 0" \
  | sed -E 's/.*\[([0-9]+)\] Capture screen 0.*/\1/' \
  | head -1)
set -o pipefail
DEVICE_INDEX=${DEVICE_INDEX:-2}

echo "== Recording for ${DURATION}s from screen device ${DEVICE_INDEX} =="
echo "   Bring OBS to foreground now — recording starts in 3s..."
sleep 3

ffmpeg -y -f avfoundation -framerate 15 -capture_cursor 1 \
  -i "${DEVICE_INDEX}:" -t "$DURATION" -pix_fmt yuv420p \
  "$TMP_MP4" 2>&1 | tail -3

echo "== Extracting frames =="
ffmpeg -y -i "$TMP_MP4" -vf "fps=15,scale=1280:-1:flags=lanczos" \
  "${TMP_FRAMES}/f_%04d.png" 2>&1 | tail -2

echo "== Encoding GIF with gifski =="
gifski --fps 15 --width 1024 -Q 80 --fast \
  -o "${OUTDIR}/hero.gif" "${TMP_FRAMES}"/*.png

echo "Done: ${OUTDIR}/hero.gif ($(du -h "${OUTDIR}/hero.gif" | awk '{print $1}'))"
