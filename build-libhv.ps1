# ============================================================================
# 构建 libhv 静态库 (Windows PowerShell, 等价 build-libhv.sh)
#
# 用法:
#   .\build-libhv.ps1 [-OpensslRoot <dir>] [-Generator 'Ninja']
#
# 前置: cmake + (ninja 或 MinGW Makefiles) + 已安装的 OpenSSL (通常由 vcpkg 提供)
#
# 产物: build_cache\libhv_install\lib\libhv_static.a (MinGW) 或 hv_static.lib (MSVC)
# ============================================================================
[CmdletBinding()]
param(
    [string]$OpensslRoot = '',
    [string]$Generator   = 'Ninja',
    [ValidateSet('mingw', 'msvc')]
    [string]$Toolchain   = 'mingw'
)

$ErrorActionPreference = 'Stop'

$ScriptDir   = $PSScriptRoot
$LibhvSrc    = Join-Path $ScriptDir 'third_party\libhv'
$BuildDir    = Join-Path $ScriptDir 'build_cache\libhv_build'
$InstallDir  = Join-Path $ScriptDir 'build_cache\libhv_install'

if (-not (Test-Path (Join-Path $LibhvSrc 'CMakeLists.txt'))) {
    Write-Error "libhv source not found at $LibhvSrc (run: git submodule update --init --recursive)"
    exit 1
}

# 增量: 已有产物则跳过
$existing = @(
    (Join-Path $InstallDir 'lib\libhv_static.a'),
    (Join-Path $InstallDir 'lib\libhv.a'),
    (Join-Path $InstallDir 'lib\hv_static.lib'),
    (Join-Path $InstallDir 'lib\hv.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($existing) {
    Write-Host "libhv 已有缓存, 跳过 ($existing)"
    exit 0
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$cmakeArgs = @(
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir",
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    "-DBUILD_SHARED=OFF",
    "-DBUILD_STATIC=ON",
    "-DWITH_OPENSSL=ON",
    "-DWITH_HTTP=ON",
    "-DWITH_HTTP_SERVER=ON",
    "-DWITH_HTTP_CLIENT=ON",
    "-DWITH_EVPP=ON",
    "-DBUILD_EXAMPLES=OFF",
    "-DBUILD_UNITTEST=OFF"
)

if ($OpensslRoot) {
    $cmakeArgs += "-DOPENSSL_ROOT_DIR=$OpensslRoot"
}

Write-Host '=== Building libhv ==='
& cmake -S $LibhvSrc -B $BuildDir -G $Generator @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --install $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$libs = Get-ChildItem (Join-Path $InstallDir 'lib') -Filter '*hv*' -ErrorAction SilentlyContinue
Write-Host 'libhv done:'
$libs | ForEach-Object { Write-Host "  $($_.FullName)" }
