#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
output_dir=${1:-/tmp/partner-oled-preview}
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
build_dir=$(mktemp -d /tmp/partner-oled-build.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
clib="$project_dir/.pio/libdeps/esp32-s3-devkitc1-n16r8/U8g2/src/clib"
python3 - "$clib" "$build_dir/fonts.c" <<'PY'
import pathlib, re, sys
source = (pathlib.Path(sys.argv[1]) / 'u8g2_fonts.c').read_text(encoding='latin1')
fonts = ['#include "u8g2.h"']
for font in ['u8g2_font_5x7_tf', 'u8g2_font_logisoso20_tn']:
    match = re.search(r'const uint8_t ' + font + r'\[.*?";\s*', source, re.S)
    assert match, font
    fonts.append(match.group())
pathlib.Path(sys.argv[2]).write_text('\n'.join(fonts))
PY
sources=(u8g2_setup u8g2_buffer u8g2_hvline u8g2_ll_hvline u8g2_intersection
  u8g2_line u8g2_box u8g2_circle u8g2_polygon u8g2_font u8g2_d_setup u8g2_d_memory
  u8x8_setup u8x8_display u8x8_cad u8x8_byte u8x8_gpio u8x8_8x8 u8x8_d_ssd1306_128x64_noname)
for source in "${sources[@]}"; do
  gcc -std=c11 -O1 -ffunction-sections -fdata-sections -I"$clib" -c "$clib/$source.c" -o "$build_dir/$source.o"
done
gcc -std=c11 -O1 -I"$clib" -c "$build_dir/fonts.c" -o "$build_dir/fonts.o"
g++ -std=c++17 -O1 -ffunction-sections -fdata-sections -Itests/host -I"$clib" -Ilib/FaceDisplay/src -Isrc \
  tests/screen_preview.cpp src/ui/ReminderScreen.cpp lib/FaceDisplay/src/*.cpp "$build_dir"/*.o \
  -Wl,--gc-sections -o "$build_dir/screen_preview"
"$build_dir/screen_preview" "$output_dir"
python3 tools/render_preview.py "$output_dir"
