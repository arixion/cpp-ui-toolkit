#!/usr/bin/env bash
# Build Skia for cpp-ui-toolkit.
#
#   tools/build_skia.sh cpu   -> third_party/skia/out/cpu   (raster only, no GPU code)
#   tools/build_skia.sh gpu   -> third_party/skia/out/gpu   (Ganesh + GL/EGL)
#
# Sources are cloned once into third_party/skia/src (pinned branch below) and shared by
# both configurations. System freetype/harfbuzz/icu/png/jpeg/webp/zlib/expat are used to
# keep the binaries small. Requires: git, python3, ninja, clang.
set -euo pipefail

SKIA_BRANCH="chrome/m157"
CONFIG="${1:-}"
case "$CONFIG" in cpu|gpu) ;; *) echo "usage: $0 <cpu|gpu>" >&2; exit 2;; esac

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SKIA_DIR="$ROOT/third_party/skia"
SRC="$SKIA_DIR/src"
OUT="$SKIA_DIR/out/$CONFIG"

if [[ ! -d "$SRC/.git" ]]; then
    echo "==> cloning Skia ($SKIA_BRANCH)"
    git clone --depth 1 --branch "$SKIA_BRANCH" https://skia.googlesource.com/skia.git "$SRC"
fi

cd "$SRC"
if [[ ! -f "$SRC/.uitk-deps-synced" ]]; then
    echo "==> syncing Skia third-party deps"
    python3 tools/git-sync-deps
    touch "$SRC/.uitk-deps-synced"
fi
[[ -x bin/gn ]] || python3 bin/fetch-gn

COMMON_ARGS='
  cc="clang" cxx="clang++"
  is_official_build=true
  is_debug=false
  extra_cflags_cc=["-frtti"]
  skia_use_system_expat=true
  skia_use_system_freetype2=true
  skia_use_system_harfbuzz=true
  skia_use_system_icu=true
  skia_use_system_libjpeg_turbo=true
  skia_use_system_libpng=true
  skia_use_system_libwebp=true
  skia_use_system_zlib=true
  skia_use_fontconfig=true
  skia_use_freetype=true
  skia_use_icu=true
  skia_use_harfbuzz=true
  skia_enable_svg=true
  skia_enable_skshaper=true
  skia_enable_skparagraph=true
  skia_enable_pdf=false
  skia_use_dng_sdk=false
  skia_use_piex=false
  skia_enable_graphite=false
  skia_use_vulkan=false
  skia_use_x11=false
'

if [[ "$CONFIG" == cpu ]]; then
    ARGS="$COMMON_ARGS skia_enable_ganesh=false skia_use_gl=false skia_use_egl=false"
else
    ARGS="$COMMON_ARGS skia_enable_ganesh=true skia_use_gl=true skia_use_egl=true"
fi

echo "==> gn gen $OUT"
bin/gn gen "$OUT" --args="$ARGS"

echo "==> ninja ($CONFIG)"
ninja -C "$OUT" skia skshaper skparagraph skunicode_core skunicode_icu svg

echo "==> done: $OUT"
ls -la "$OUT"/*.a
