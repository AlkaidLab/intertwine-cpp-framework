# ============================================================================
# intertwine_cpp_framework — Windows PowerShell build script (equivalent to build.sh)
#
# Usage:
#   .\build.ps1
#   .\build.ps1 -VcpkgRoot D:\vcpkg
#   .\build.ps1 -InstallDir D:\out\intertwine_cpp_framework_install
#   .\build.ps1 -Test
#   .\build.ps1 -Clean
#
# Stages:
#   [1/4] Prepare vcpkg and install dependencies (Boost / OpenSSL / spdlog / json)
#   [2/4] Build libhv (requires OpenSSL)
#   [3/4] Build the framework (requires libhv and Boost)
#   [4/4] Install to -InstallDir or run tests
#
# Prerequisites:
#   - cmake, ninja, and git must be on PATH.
#   - MinGW: gcc and g++ must be on PATH (MSYS2 mingw64 is recommended).
#   - MSVC: start pwsh from an "x64 Native Tools" environment.
# ============================================================================
[CmdletBinding()]
param(
    [string]$VcpkgRoot          = '',
    [string]$VcpkgInstalledDir  = '',
    [string]$InstallDir         = '',
    [ValidateSet('x64-mingw-dynamic', 'x64-mingw-static', 'x64-windows', 'x64-windows-static')]
    [string]$Triplet            = 'x64-mingw-dynamic',
    [string]$Generator          = 'Ninja',
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$FwDir         = $PSScriptRoot
$BuildDir      = Join-Path $FwDir 'build'
$LibhvInstall  = Join-Path $FwDir 'build_cache\libhv_install'

if (-not $InstallDir) {
    $projectRoot = Resolve-Path (Join-Path $FwDir '..\..')
    $InstallDir  = Join-Path $projectRoot.Path 'build_cache\intertwine_cpp_framework_install'
}

Write-Host '================================================'
Write-Host '  Building intertwine_cpp_framework (Windows)'
Write-Host '================================================'

# ── [1/4] Prepare vcpkg ──
if ($VcpkgRoot) {
    $VcpkgDir = $VcpkgRoot
    if (-not (Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake'))) {
        Write-Error "Invalid -VcpkgRoot path: $VcpkgDir"
        exit 1
    }
    Write-Host "[1/4] Reusing vcpkg: $VcpkgDir"
} else {
    $VcpkgDir = Join-Path $FwDir 'vcpkg'
    if (-not (Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake'))) {
        Write-Host "[1/4] vcpkg not found; cloning to $VcpkgDir..."
        & git clone https://github.com/microsoft/vcpkg.git $VcpkgDir
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & (Join-Path $VcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
$VcpkgToolchain = Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake'
$VcpkgExe       = Join-Path $VcpkgDir 'vcpkg.exe'

if (-not $VcpkgInstalledDir) {
    $VcpkgInstalledDir = Join-Path $BuildDir 'vcpkg_installed'
}
$OpensslRoot = Join-Path $VcpkgInstalledDir $Triplet

# Check for OpenSSL; library names vary by triplet.
$opensslMarker = @(
    (Join-Path $OpensslRoot 'lib\libssl.a'),
    (Join-Path $OpensslRoot 'lib\libssl.lib'),
    (Join-Path $OpensslRoot 'lib\ssl.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $opensslMarker) {
    Write-Host '  Installing vcpkg dependencies...'
    & $VcpkgExe install `
        --triplet=$Triplet `
        --x-install-root=$VcpkgInstalledDir `
        --x-manifest-root=$FwDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$opensslMarker = @(
    (Join-Path $OpensslRoot 'lib\libssl.a'),
    (Join-Path $OpensslRoot 'lib\libssl.lib'),
    (Join-Path $OpensslRoot 'lib\ssl.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $opensslMarker) {
    Write-Error 'OpenSSL installation failed; check the vcpkg logs.'
    exit 1
}
Write-Host '  vcpkg dependencies are ready'

# ── [2/4] Build libhv ──
$libhvLib = @(
    (Join-Path $LibhvInstall 'lib\libhv_static.a'),
    (Join-Path $LibhvInstall 'lib\libhv.a'),
    (Join-Path $LibhvInstall 'lib\hv_static.lib'),
    (Join-Path $LibhvInstall 'lib\hv.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $libhvLib) {
    Write-Host '[2/4] Building libhv...'
    & (Join-Path $FwDir 'build-libhv.ps1') -OpensslRoot $OpensslRoot -Generator $Generator
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $libhvLib = @(
        (Join-Path $LibhvInstall 'lib\libhv_static.a'),
        (Join-Path $LibhvInstall 'lib\libhv.a'),
        (Join-Path $LibhvInstall 'lib\hv_static.lib'),
        (Join-Path $LibhvInstall 'lib\hv.lib')
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
} else {
    Write-Host '[2/4] Reusing cached libhv'
}
if (-not $libhvLib) { Write-Error 'libhv build failed'; exit 1 }
Write-Host "  libhv is ready: $libhvLib"

# ── [3/4] Build the framework ──
Write-Host '[3/4] Building the framework...'
if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
}

$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $FwDir,
    "-G", $Generator,
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",
    "-DVCPKG_INSTALLED_DIR=$VcpkgInstalledDir",
    "-DVCPKG_TARGET_TRIPLET=$Triplet",
    "-DVCPKG_INSTALL_OPTIONS=--no-print-usage",
    "--log-level=NOTICE",
    "-DLIBHV_INCLUDE_DIR=$(Join-Path $LibhvInstall 'include')",
    "-DLIBHV_LIB=$libhvLib",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir"
)

if ($Test -and $env:GTEST_PREFIX) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$env:GTEST_PREFIX"
}

# Skip configuration for an incremental build when the installation prefix matches.
$needConfigure = $true
$cacheFile = Join-Path $BuildDir 'CMakeCache.txt'
if ((Test-Path $cacheFile) -and -not $Clean) {
    $cachedPrefix = (Select-String -Path $cacheFile -Pattern '^CMAKE_INSTALL_PREFIX:PATH=(.*)$' |
                     Select-Object -First 1).Matches.Groups[1].Value
    if ($cachedPrefix -eq $InstallDir) { $needConfigure = $false }
}

if ($needConfigure) {
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& cmake --build $BuildDir --config Release -j ([Environment]::ProcessorCount)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# ── [4/4] Install or test ──
if ($Test) {
    Write-Host '[4/4] Running tests...'
    Push-Location $BuildDir
    try {
        & ctest --output-on-failure
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally { Pop-Location }
} else {
    Write-Host "[4/4] Installing to $InstallDir..."
    & cmake --install $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host ''
Write-Host "Installation complete: $InstallDir"
