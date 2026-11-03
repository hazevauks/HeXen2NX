#!/usr/bin/env python3
"""Copies the game's files from your own copy of HeXen II into SD_CARD/.

    python tools/package_sd.py "C:/Games/HeXen II"

Names are made lowercase and the CD music goes where the engine looks for it,
under the name the game asks for (the same as source/platform/nx_data.c does on
the console for a copy that was placed by hand). A PC's settings stay behind.
"""

import argparse
import shutil
import struct
import sys
from pathlib import Path

GAME_FOLDER = Path("SD_CARD/switch/HeXen2NX")

MUSIC_NAMES = {
    "data1": {
        2: "casa1", 3: "casa2", 4: "casa3", 5: "casa4",
        6: "egyp1", 7: "egyp2", 8: "egyp3",
        9: "meso1", 10: "meso2", 11: "meso3",
        12: "roma1", 13: "roma2", 14: "roma3",
        15: "casb1", 16: "casb2", 17: "casb3",
    },
    "portals": {
        2: "tulku7", 3: "tulku1", 4: "tulku4", 5: "tulku2",
        6: "tulku9", 7: "tulku10", 8: "tulku6", 9: "tulku5",
        10: "tulku8", 11: "tulku3",
    },
}

# The two retail versions of the base game, as uHexen2's filesystem checks them.
KNOWN_PAKS = {"pak0.pak": 22704056, "pak1.pak": 75601170}

SKIPPED_NAMES = {"config.cfg", "autoexec.cfg", "glhexen"}


def copy_if_changed(source: Path, target: Path) -> bool:
    if target.exists() and target.stat().st_size == source.stat().st_size:
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    return True


def check_pak(path: Path) -> int:
    with path.open("rb") as pak:
        magic, _, size = struct.unpack("<4sii", pak.read(12))
    if magic != b"PACK":
        sys.exit(f"{path} is not a pak file")
    return size // 64


def copy_game_folder(game: Path, name: str, output: Path) -> int:
    source = next((p for p in game.iterdir() if p.is_dir() and p.name.lower() == name), None)
    if source is None:
        return 0

    copied = 0
    for entry in sorted(source.iterdir()):
        lower = entry.name.lower()
        if not entry.is_file() or lower in SKIPPED_NAMES:
            continue
        copied += copy_if_changed(entry, output / name / lower)
        if lower.endswith(".pak"):
            print(f"  {name}/{lower}: {check_pak(entry)} files, {entry.stat().st_size} bytes")
            expected = KNOWN_PAKS.get(lower) if name == "data1" else None
            if expected is not None and expected != entry.stat().st_size:
                print(f"  warning: {lower} is not the 1.11 version: patch your copy first")
    return copied


def music_sources(game: Path, name: str) -> list[Path]:
    """GOG's copy keeps the base game's tracks in a folder next to data1."""
    folders = [game / name / "music"]
    if name == "data1":
        folders.append(game / "music")
    return [folder for folder in folders if folder.is_dir()]


def copy_music(game: Path, name: str, output: Path) -> int:
    copied = 0
    for folder in music_sources(game, name):
        for entry in sorted(folder.iterdir()):
            lower = entry.name.lower()
            if not (lower.startswith("track") and lower.endswith(".ogg")):
                continue
            midi_name = MUSIC_NAMES[name].get(int(lower[5:7]))
            if midi_name:
                copied += copy_if_changed(entry, output / name / "music" / f"{midi_name}.ogg")
    return copied


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("game", type=Path, help="the folder of your copy of the game")
    args = parser.parse_args()

    output = Path(__file__).resolve().parent.parent / GAME_FOLDER
    if not (args.game / "data1").is_dir() and not any(
        p.name.lower() == "data1" for p in args.game.iterdir()
    ):
        sys.exit(f"no data1 folder in {args.game}")

    for name in MUSIC_NAMES:
        files = copy_game_folder(args.game, name, output)
        tracks = copy_music(args.game, name, output)
        if files or tracks:
            print(f"{name}: {files} files and {tracks} music tracks copied")

    (output / "SD_LOG").mkdir(parents=True, exist_ok=True)
    print(f"ready: {output}")


if __name__ == "__main__":
    main()
