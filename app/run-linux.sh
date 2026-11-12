#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(dirname -- "$app_dir")"
qt_root="${GUITAR_QT_ROOT:-$project_dir/.codex_tmp/QtLinux/6.8.3/gcc_64}"
build_dir="$app_dir/build-linux"
if [[ -d "$qt_root/lib/cmake/Qt6" ]]; then
    export QT_PLUGIN_PATH="$qt_root/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QML_IMPORT_PATH="$qt_root/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
    qt_args=("-DCMAKE_PREFIX_PATH=$qt_root")
else
    qt_args=()
fi
cmake -S "$app_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug "${qt_args[@]}"
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
if [[ "${1:-}" == "--check" ]]; then exit 0; fi
exec "$build_dir/SelfGuitar" "$@"
