#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: scripts/build.sh [options]

Options:
  --with-tests     Configure and build test targets.
  --no-tests       Configure without test targets. This is the default.
  --run-tests      Configure, build, and run tests after building.
  -h, --help       Show this help.

Environment:
  QT_ROOT          Qt install root, for example /path/to/Qt/6.x.x/mingw_64
  MINGW_BIN        MinGW bin dir, for example /path/to/Qt/Tools/mingw/bin
  BUILD_DIR        Build directory. Defaults to <project>/build
  GENERATOR        CMake generator. Defaults to "MinGW Makefiles"
  JOBS             Build parallelism. Defaults to 8
  BUILD_TESTS      ON or OFF. Defaults to OFF
EOF
}

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
BUILD_TESTS="${BUILD_TESTS:-OFF}"
RUN_TESTS="${RUN_TESTS:-OFF}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-tests)
            BUILD_TESTS=ON
            ;;
        --no-tests)
            BUILD_TESTS=OFF
            ;;
        --run-tests)
            BUILD_TESTS=ON
            RUN_TESTS=ON
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

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
case "$BUILD_DIR_SHELL" in
    /*) ;;
    *) BUILD_DIR_SHELL="$ROOT_DIR/$BUILD_DIR_SHELL" ;;
esac

export PATH="$MINGW_BIN_SHELL:$QT_ROOT_SHELL/bin:$PATH"

echo "Project: $ROOT_DIR"
echo "Qt:      $QT_ROOT"
echo "Build:   $BUILD_DIR_SHELL"
echo "Tests:   $BUILD_TESTS"

if [[ -f "$BUILD_DIR_SHELL/CMakeCache.txt" ]]; then
    cached_cxx="$(grep -E '^CMAKE_CXX_COMPILER:' "$BUILD_DIR_SHELL/CMakeCache.txt" | cut -d= -f2- || true)"
    cached_cxx="$(to_shell_path "$cached_cxx")"
    expected_cxx="$MINGW_BIN_SHELL/g++.exe"
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
    -DCMAKE_PREFIX_PATH="$QT_ROOT_SHELL" \
    -DCMAKE_CXX_COMPILER="$MINGW_BIN_SHELL/g++.exe" \
    -DCMAKE_MAKE_PROGRAM="$MINGW_BIN_SHELL/mingw32-make.exe" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTING="$BUILD_TESTS"

if [[ "$BUILD_TESTS" == "ON" ]]; then
    cmake --build "$BUILD_DIR_SHELL" -j "$JOBS"
else
    cmake --build "$BUILD_DIR_SHELL" --target imgmgr -j "$JOBS"
fi

if [[ "$RUN_TESTS" == "ON" ]]; then
    ctest --test-dir "$BUILD_DIR_SHELL" --output-on-failure
fi

(
    cd "$BUILD_DIR_SHELL"
    windeployqt imgmgr.exe
)

echo
echo "Build completed: $BUILD_DIR_SHELL/imgmgr.exe"
