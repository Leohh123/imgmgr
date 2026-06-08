#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ENV_FILE="${ENV_FILE:-$ROOT_DIR/.env}"
if [[ -f "$ENV_FILE" ]]; then
    set -a
    # shellcheck source=/dev/null
    source "$ENV_FILE"
    set +a
fi

QT_ROOT="${QT_ROOT:-}"
MINGW_BIN="${MINGW_BIN:-}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
GENERATOR="${GENERATOR:-MinGW Makefiles}"
JOBS="${JOBS:-8}"

if [[ -z "$QT_ROOT" || -z "$MINGW_BIN" ]]; then
    cat >&2 <<'EOF'
Missing build configuration.

Please copy .env.example to .env and set:
  QT_ROOT=/path/to/Qt/6.x.x/<compiler>
  MINGW_BIN=/path/to/Qt/Tools/<mingw>/bin

You can also pass them as environment variables when running this script.
EOF
    exit 1
fi

to_shell_path() {
    local path="$1"
    case "$path" in
        [a-zA-Z]:/* | [a-zA-Z]:\\*)
            local drive="${path:0:1}"
            local rest="${path:2}"
            drive="$(printf '%s' "$drive" | tr '[:upper:]' '[:lower:]')"
            rest="${rest//\\//}"
            printf '/%s%s\n' "$drive" "$rest"
            ;;
        *)
            printf '%s\n' "$path"
            ;;
    esac
}

QT_ROOT_SHELL="$(to_shell_path "$QT_ROOT")"
MINGW_BIN_SHELL="$(to_shell_path "$MINGW_BIN")"
BUILD_DIR_SHELL="$(to_shell_path "$BUILD_DIR")"

export PATH="$MINGW_BIN_SHELL:$QT_ROOT_SHELL/bin:$PATH"

echo "Project: $ROOT_DIR"
echo "Qt:      $QT_ROOT"
echo "Build:   $BUILD_DIR"

if [[ -f "$BUILD_DIR_SHELL/CMakeCache.txt" ]]; then
    cached_cxx="$(grep -E '^CMAKE_CXX_COMPILER:' "$BUILD_DIR_SHELL/CMakeCache.txt" | cut -d= -f2- || true)"
    expected_cxx="$MINGW_BIN/g++.exe"
    if [[ -n "$cached_cxx" && "$cached_cxx" != "$expected_cxx" ]]; then
        case "$BUILD_DIR_SHELL" in
            "$ROOT_DIR"/*)
                echo "Cached compiler differs from Qt MinGW; recreating build directory."
                rm -rf "$BUILD_DIR_SHELL"
                ;;
            *)
                echo "Refusing to remove build directory outside project: $BUILD_DIR_SHELL" >&2
                exit 1
                ;;
        esac
    fi
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR_SHELL" -G "$GENERATOR" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DCMAKE_C_COMPILER="$MINGW_BIN/gcc.exe" \
    -DCMAKE_CXX_COMPILER="$MINGW_BIN/g++.exe" \
    -DCMAKE_MAKE_PROGRAM="$MINGW_BIN/mingw32-make.exe" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR_SHELL" -j "$JOBS"

(
    cd "$BUILD_DIR_SHELL"
    windeployqt imgmgr.exe
)

echo
echo "Build completed: $BUILD_DIR/imgmgr.exe"
