#!/usr/bin/env bash
# Fetch sherpa-onnx prebuilt archives listed in buildspec.json, compute SHA-256,
# and print a snippet to paste back into `dependencies["sherpa-onnx"]["hashes"]`.
#
# Usage: scripts/compute-sherpa-onnx-hashes.sh
set -euo pipefail

BUILDSPEC="${1:-buildspec.json}"
VERSION=$(jq -r '.dependencies["sherpa-onnx"].version' "$BUILDSPEC")
BASE=$(jq -r '.dependencies["sherpa-onnx"].baseUrl' "$BUILDSPEC")

echo "sherpa-onnx version: ${VERSION}"
echo

for platform in macos windows-x64; do
    asset=$(jq -r ".dependencies[\"sherpa-onnx\"].assets[\"${platform}\"]" "$BUILDSPEC")
    url="${BASE}/v${VERSION}/${asset}"
    echo "fetching ${url}"
    tmp=$(mktemp)
    curl -L -sSf -o "$tmp" "$url"
    sha=$(shasum -a 256 "$tmp" | awk '{print $1}')
    rm -f "$tmp"
    printf '        "%s": "%s",\n' "$platform" "$sha"
done
