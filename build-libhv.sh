#!/bin/bash
# ============================================================================
# Build the libhv static library.
# Usage: bash build-libhv.sh [--openssl-root <dir>]
#
# Environment:
#   WITH_IO_URING=1    Enable the io_uring event-loop backend (Linux 5.1+ and liburing-devel required)
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBHV_DIR="$SCRIPT_DIR/third_party/libhv"
BUILD_DIR="$SCRIPT_DIR/build_cache/libhv_build"
INSTALL_DIR="$SCRIPT_DIR/build_cache/libhv_install"
BUILD_STAMP="$INSTALL_DIR/.build-stamp"

OPENSSL_ROOT=""

# Parse arguments.
while [[ $# -gt 0 ]]; do
    case "$1" in
        --openssl-root) OPENSSL_ROOT="$2"; shift 2 ;;
        *)              shift ;;
    esac
done

if [ ! -f "$LIBHV_DIR/CMakeLists.txt" ]; then
    echo "Error: libhv source not found at $LIBHV_DIR"
    echo "Run: git submodule update --init --recursive"
    exit 1
fi

# Reuse an archive only when it was built from the current libhv source.
# This covers both a new submodule commit and local source changes while
# preventing stale headers and static libraries from being mixed.
SOURCE_REV="$(git -C "$LIBHV_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
SOURCE_DIFF="$(git -C "$LIBHV_DIR" diff --binary -- . 2>/dev/null | sha256sum | awk '{print $1}')"
BUILD_SIGNATURE="source=$SOURCE_REV;diff=$SOURCE_DIFF;openssl=$OPENSSL_ROOT;io_uring=${WITH_IO_URING:-0}"
if [[ -f "$BUILD_STAMP" && \
      ( -f "$INSTALL_DIR/lib/libhv_static.a" || -f "$INSTALL_DIR/lib/libhv.a" ) && \
      "$(cat "$BUILD_STAMP")" = "$BUILD_SIGNATURE" ]]; then
    echo "Reusing cached libhv ($INSTALL_DIR)"
    exit 0
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED=OFF
    -DBUILD_STATIC=ON
    -DWITH_OPENSSL=ON
    -DWITH_HTTP=ON
    -DWITH_HTTP_SERVER=ON
    -DWITH_HTTP_CLIENT=ON
    -DWITH_EVPP=ON
    -DBUILD_EXAMPLES=OFF
    -DBUILD_UNITTEST=OFF
)

if [ -n "$OPENSSL_ROOT" ]; then
    CMAKE_ARGS+=(-DOPENSSL_ROOT_DIR="$OPENSSL_ROOT")
fi

# io_uring event-loop backend (Linux 5.1+ and liburing-devel required).
if [ "${WITH_IO_URING:-0}" = "1" ]; then
    if ! find /usr/include /usr/local/include -name "liburing.h" -print -quit 2>/dev/null | grep -q .; then
        echo "Error: liburing-devel not found. Install it first:"
        echo "  RHEL/CentOS: dnf install liburing-devel"
        echo "  Debian/Ubuntu: apt install liburing-dev"
        exit 1
    fi
    CMAKE_ARGS+=(-DWITH_IO_URING=ON)
    echo "io_uring backend: ENABLED"
else
    echo "io_uring backend: disabled (set WITH_IO_URING=1 to enable)"
fi

echo "=== Building libhv ==="
cmake -B "$BUILD_DIR" -S "$LIBHV_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"
cmake --install "$BUILD_DIR"
printf '%s\n' "$BUILD_SIGNATURE" > "$BUILD_STAMP"

echo "libhv done:"
ls -lh "$INSTALL_DIR/lib/"*hv* 2>/dev/null
