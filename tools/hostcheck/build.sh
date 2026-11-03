#!/usr/bin/env bash
# Builds the port for the PC it runs on, with tools/hostcheck/shim/ standing in
# for libnx. Needs a POSIX environment with GCC (MSYS2's "gcc", or any Linux).
#
#   tools/hostcheck/build.sh <game folder> [extra compiler flags]
#
# The game folder is laid out as on the SD card; it is compiled in as the base
# directory, like the console's path, and written to (settings, saves, logs).
set -e

game_dir="$1"
shift
extra_flags=("$@")
[ -n "$game_dir" ] || { echo "usage: $0 <game folder> [flags]"; exit 2; }

root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$root/build_host"
mkdir -p "$out/obj"

# The source lists are CMakeLists.txt's. The console's video and crash code goes
# in by hand, the vorbis decoder stays out: there is no Tremor for this machine.
sources=$(sed -n '/^set(ENGINE_SOURCES/,/^)/p;/^set(SOFTWARE_SOURCES/,/^)/p;/^set(PLATFORM_SOURCES/,/^)/p' "$root/CMakeLists.txt" \
	| grep -E '\.c$' | sed -E "s/^[[:space:]]+//;
		s#\\\$\{H2\}#third_party/uhexen2/engine/hexen2#;
		s#\\\$\{SHARED\}#third_party/uhexen2/engine/h2shared#;
		s#\\\$\{COMMON\}#third_party/uhexen2/common#" \
	| grep -vE 'nx_crash\.c|snd_vorbis\.c')
sources="$sources source/platform/vid_nx.c tools/hostcheck/shim/switch_shim.c tools/hostcheck/host_main.c"

flags=(-std=gnu11 -O1 -g -fno-strict-aliasing -fno-omit-frame-pointer
	-D__SWITCH__ -DNX_HOSTCHECK -DNX_GAME_DIR="\"$game_dir\"" -DUSE_CODEC_WAVE
	-D__PDP_ENDIAN=3412
	-I"$root/tools/hostcheck/shim" -I"$root/source/platform"
	-I"$root/third_party/uhexen2/engine/hexen2" -I"$root/third_party/uhexen2/engine/h2shared"
	-I"$root/third_party/uhexen2/common" -w "${extra_flags[@]}")

objects=()
cd "$root"
for src in $sources; do
	obj="$out/obj/$(basename "$src" .c).o"
	objects+=("$obj")
	if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$FORCE" ]; then
		echo "gcc $src"
		gcc "${flags[@]}" -c "$src" -o "$obj" &
		while [ "$(jobs -r | wc -l)" -ge "${JOBS:-4}" ]; do wait -n; done
	fi
done
wait
gcc "${objects[@]}" "${extra_flags[@]}" -lm -lpthread -o "$out/hexen2_host"
echo "built $out/hexen2_host"
