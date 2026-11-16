"""Apply or check the project's pinned C++ and QML formatting rules."""

import argparse
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def source_files():
    directories = [
        ROOT / "core",
        ROOT / "desktop/src",
        ROOT / "shared",
        ROOT / "tests",
        ROOT / "firmware/song_receiver/src",
    ]
    for directory in directories:
        for path in sorted(directory.rglob("*")):
            if path.suffix in {".cpp", ".h"}:
                yield path, "clang-format-18"

    yield ROOT / "desktop/qml/Main.qml", "qmlformat"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true")
    options = parser.parse_args()

    version = subprocess.check_output(["clang-format-18", "--version"], text=True)
    if "version 18." not in version:
        parser.error("LLVM 18 is required; enter the project's nix develop shell")

    changed = []
    for path, formatter in source_files():
        formatted = subprocess.check_output([formatter, str(path)])
        current = path.read_bytes()
        if current == formatted:
            continue

        changed.append(path.relative_to(ROOT))
        if options.fix:
            path.write_bytes(formatted)

    for path in changed:
        print(f"{'Formatted' if options.fix else 'Needs formatting'}: {path}")

    return int(bool(changed) and not options.fix)


if __name__ == "__main__":
    raise SystemExit(main())
