#!/bin/bash
# ============================================================================
# intertwine_cpp_framework — self-contained build script
# ============================================================================
# Usage:
#   ./build.sh                                    # Standalone build; clones vcpkg when needed
#   ./build.sh --vcpkg-root /path/to/vcpkg        # Reuse an existing vcpkg checkout
#   ./build.sh --vcpkg-installed-dir /path/to/dir # Override the vcpkg_installed directory
#   ./build.sh --install-dir /path/to/out         # Override the installation directory
#   ./build.sh --triplet x64-mingw-dynamic        # Override the detected vcpkg triplet
#   ./build.sh --test                             # Build and run tests
#   ./build.sh --clean                            # Rebuild from a clean build directory
#
# Build stages (each stage depends on the preceding stages):
#   [1/4] Prepare vcpkg and install dependencies (Boost/OpenSSL/spdlog/json)
#   [2/4] Build libhv (requires OpenSSL from stage 1)
#   [3/4] Build the framework (requires libhv from stage 2 and Boost from stage 1)
#   [4/4] Install to --install-dir or run tests
#
# Environment:
#   GTEST_PREFIX — Directory containing the GTest CMake package config (for --test)
# ============================================================================
set -e

FW_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$FW_DIR/build"
LIBHV_INSTALL="$FW_DIR/build_cache/libhv_install"

# Defaults
INSTALL_DIR=""
VCPKG_ROOT_OVERRIDE=""
VCPKG_INSTALLED_OVERRIDE=""
VCPKG_TRIPLET=""
RUN_TESTS=false
DO_CLEAN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --install-dir)          INSTALL_DIR="$2"; shift 2 ;;
        --vcpkg-root)           VCPKG_ROOT_OVERRIDE="$2"; shift 2 ;;
        --vcpkg-installed-dir)  VCPKG_INSTALLED_OVERRIDE="$2"; shift 2 ;;
        --triplet)              VCPKG_TRIPLET="$2"; shift 2 ;;
        --test)                 RUN_TESTS=true; shift ;;
        --clean)                DO_CLEAN=true; shift ;;
        *)                      echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Default installation directory
if [[ -z "$INSTALL_DIR" ]]; then
    PROJECT_ROOT="$(cd "$FW_DIR/../.." && pwd)"
    INSTALL_DIR="$PROJECT_ROOT/build_cache/intertwine_cpp_framework_install"
fi

echo "================================================"
echo "  Building intertwine_cpp_framework"
echo "================================================"

# ── [1/4] Locate or bootstrap vcpkg ──
if [[ -n "$VCPKG_ROOT_OVERRIDE" ]]; then
    VCPKG_DIR="$VCPKG_ROOT_OVERRIDE"
    if [[ ! -f "$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" ]]; then
        echo "Error: invalid --vcpkg-root path: $VCPKG_DIR"
        exit 1
    fi
    echo "[1/4] Reusing vcpkg: $VCPKG_DIR"
else
    VCPKG_DIR="$FW_DIR/vcpkg"
    if [[ ! -f "$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" ]]; then
        echo "[1/4] vcpkg not found; cloning to $VCPKG_DIR..."
        git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
        "$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics
    fi
fi
VCPKG_TOOLCHAIN="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"

# vcpkg_installed directory
if [[ -n "$VCPKG_INSTALLED_OVERRIDE" ]]; then
    VCPKG_INSTALLED="$VCPKG_INSTALLED_OVERRIDE"
else
    VCPKG_INSTALLED="$BUILD_DIR/vcpkg_installed"
fi

# vcpkg triplet (detected from the host OS and architecture by default)
if [[ -z "$VCPKG_TRIPLET" ]]; then
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) VCPKG_TRIPLET="x64-mingw-dynamic" ;;
        *)
            case "$(uname -m)" in
                aarch64|arm64) VCPKG_TRIPLET="arm64-linux" ;;
                *)             VCPKG_TRIPLET="x64-linux" ;;
            esac
            ;;
    esac
fi
OPENSSL_ROOT="$VCPKG_INSTALLED/$VCPKG_TRIPLET"

# Check for OpenSSL: static builds use libssl.a; MinGW dynamic triplets use libssl.dll.a.
_openssl_ok() { ls "$OPENSSL_ROOT/lib/libssl"* 2>/dev/null | grep -q .; }

if ! _openssl_ok; then
    echo "  Installing vcpkg dependencies..."
    "$VCPKG_DIR/vcpkg" install \
        --triplet="$VCPKG_TRIPLET" \
        --x-install-root="$VCPKG_INSTALLED" \
        --x-manifest-root="$FW_DIR"
fi

if ! _openssl_ok; then
    echo "Error: OpenSSL installation failed; check the vcpkg logs."
    exit 1
fi
echo "  vcpkg dependencies are ready"

# ── [2/4] Build libhv ──
if [[ ! -f "$LIBHV_INSTALL/lib/libhv_static.a" && \
      ! -f "$LIBHV_INSTALL/lib/libhv.a" ]]; then
    echo "[2/4] Building libhv..."
    bash "$FW_DIR/build-libhv.sh" --openssl-root "$OPENSSL_ROOT"
else
    echo "[2/4] Reusing cached libhv"
fi

# Locate the produced libhv archive.
LIBHV_LIB=""
for candidate in "$LIBHV_INSTALL/lib/libhv_static.a" \
                 "$LIBHV_INSTALL/lib/libhv.a"; do
    [[ -f "$candidate" ]] && LIBHV_LIB="$candidate" && break
done
if [[ -z "$LIBHV_LIB" ]]; then
    echo "Error: libhv build failed"
    exit 1
fi
echo "  libhv is ready: $LIBHV_LIB"

# ── [3/4] Build the framework ──
echo "[3/4] Building the framework..."
if $DO_CLEAN && [[ -d "$BUILD_DIR" ]]; then
    rm -rf "$BUILD_DIR"
fi

CMAKE_ARGS=(
    -B "$BUILD_DIR" -S "$FW_DIR"
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN"
    -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
    -DVCPKG_INSTALLED_DIR="$VCPKG_INSTALLED"
    -DVCPKG_INSTALL_OPTIONS="--no-print-usage"
    --log-level=NOTICE
    -DLIBHV_INCLUDE_DIR="$LIBHV_INSTALL/include"
    -DLIBHV_LIB="$LIBHV_LIB"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
)

# Test mode: provide GTest to CMake when supplied.
if $RUN_TESTS; then
    echo "  Tests: enabled"
    if [[ -n "$GTEST_PREFIX" ]]; then
        CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$GTEST_PREFIX")
    fi
fi

# Skip configuration for an incremental build when the installation prefix matches.
NEED_CONFIGURE=true
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && ! $DO_CLEAN; then
    CACHED_PREFIX=$(grep 'CMAKE_INSTALL_PREFIX:PATH=' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2)
    if [[ "$CACHED_PREFIX" = "$INSTALL_DIR" ]]; then
        NEED_CONFIGURE=false
    fi
fi

if $NEED_CONFIGURE; then
    cmake "${CMAKE_ARGS[@]}" 2>&1
fi

NPROC=$(nproc 2>/dev/null || echo 4)
cmake --build "$BUILD_DIR" -j"$NPROC" 2>&1

# ── [4/4] Install or test ──
if $RUN_TESTS; then
    echo "[4/4] Running tests..."
    cd "$BUILD_DIR" && ctest --output-on-failure 2>&1
else
    echo "[4/4] Installing to $INSTALL_DIR..."
    cmake --install "$BUILD_DIR" 2>&1
fi

echo ""
echo "Installation complete: $INSTALL_DIR"
