#!/usr/bin/env bash
set -euo pipefail
software_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v nix >/dev/null 2>&1; then
    echo 'Nix не найден. Установите Qt 6.8+ и используйте bash software/tools/run-linux.sh.' >&2
    exit 1
fi

cd "$software_dir"
exec nix develop --command bash tools/run-linux.sh "$@"
