#!/usr/bin/env bash
# Configures and builds the Switch binaries. Run it in devkitPro's MSYS2, or in
# the devkitpro/devkita64 container: anywhere DEVKITPRO is set.
set -e
: "${DEVKITPRO:?DEVKITPRO is not set}"
cd "$(dirname "$0")"
"${DEVKITPRO}/portlibs/switch/bin/aarch64-none-elf-cmake" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=None
cmake --build build "$@"
