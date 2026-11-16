#!/usr/bin/env bash
set -euo pipefail
software_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
preset="${GUITAR_PRESET:-debug}"
build_dir="$software_dir/build/$preset"
build=false
check=false
case "${1:-}" in
    --build) build=true; shift ;;
    --check) build=true; check=true; shift ;;
esac
cd "$software_dir"

if $build || [[ ! -x "$build_dir/bin/SelfGuitar" ]]; then
    echo 'Сборка SelfGuitar… После сборки откроется окно приложения.'
    cmake --preset "$preset"
    cmake --build --preset "$preset"
    ctest --preset "$preset"
fi
if $check; then exit 0; fi
# WSLg can display the window while the Nix Qt build cannot use the host EGL
# driver. Use Qt Quick's software renderer unless explicitly overridden.
IFS= read -r kernel_release < /proc/sys/kernel/osrelease
if [[ "${kernel_release,,}" == *microsoft* ]]; then
    export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
    if [[ -n "${DISPLAY:-}" ]]; then
        export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
    fi
fi
echo 'Запуск SelfGuitar…'
exec "$build_dir/bin/SelfGuitar" "$@"
