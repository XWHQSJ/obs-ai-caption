#!/bin/zsh
# Deploys the built obs-ai-caption.plugin into a locally built OBS.app.
# Expects:
#   OBS_APP=<path to OBS.app>  (e.g. /Users/.../obs-studio/build_macos/UI/RelWithDebInfo/OBS.app)
#   PLUGIN_DIR=<path to plugin build dir> (defaults to build-plugin-macos)
set -euo pipefail

: ${OBS_APP:="/Users/bytedance/Workspace/obs-studio/build_macos/UI/RelWithDebInfo/OBS.app"}
: ${PLUGIN_DIR:="/Users/bytedance/Workspace/obs-ai-caption/build-plugin-macos"}

if [[ ! -d "${OBS_APP}" ]]; then
  echo "OBS.app not found at ${OBS_APP}" >&2
  exit 1
fi

PLUGIN_BUNDLE="${PLUGIN_DIR}/obs-ai-caption.plugin"
if [[ ! -d "${PLUGIN_BUNDLE}" ]]; then
  echo "Plugin bundle not found at ${PLUGIN_BUNDLE} — build first" >&2
  exit 1
fi

PLUGINS_DIR="${OBS_APP}/Contents/PlugIns"
DATA_DIR="${OBS_APP}/Contents/Resources/data/obs-plugins/obs-ai-caption"

mkdir -p "${PLUGINS_DIR}" "${DATA_DIR}"

echo "Copying plugin bundle..."
rm -rf "${PLUGINS_DIR}/obs-ai-caption.plugin"
cp -R "${PLUGIN_BUNDLE}" "${PLUGINS_DIR}/"

echo "Copying data..."
cp -R /Users/bytedance/Workspace/obs-ai-caption/data/. "${DATA_DIR}/"

echo "Ad-hoc signing..."
codesign --force --deep --sign - "${PLUGINS_DIR}/obs-ai-caption.plugin"

echo "Deployed to ${PLUGINS_DIR}/obs-ai-caption.plugin"
