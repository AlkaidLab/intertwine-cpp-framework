# ============================================================================
# alkaidlab_fw — Windows PowerShell 构建脚本 (等价 build.sh)
#
# 用法:
#   .\build.ps1
#   .\build.ps1 -VcpkgRoot D:\vcpkg
#   .\build.ps1 -InstallDir D:\out\alkaidlab_fw_install
#   .\build.ps1 -Test
#   .\build.ps1 -Clean
#
# 流程:
#   [1/4] vcpkg 准备 + 安装依赖 (Boost / OpenSSL / spdlog / json)
#   [2/4] 构建 libhv (依赖 OpenSSL)
#   [3/4] 构建 fw 库 (依赖 libhv + Boost)
#   [4/4] 安装到 -InstallDir
#
# 前置:
#   - cmake, ninja, git 在 PATH
#   - MinGW 模式: gcc / g++ 在 PATH (推荐 MSYS2 mingw64)
#   - MSVC 模式: 在 "x64 Native Tools" 环境中启动 pwsh
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
    $InstallDir  = Join-Path $projectRoot.Path 'build_cache\alkaidlab_fw_install'
}

Write-Host '================================================'
Write-Host '  alkaidlab_fw 构建 (Windows)'
Write-Host '================================================'

# ── [1/4] vcpkg 准备 ──
if ($VcpkgRoot) {
    $VcpkgDir = $VcpkgRoot
    if (-not (Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake'))) {
        Write-Error "-VcpkgRoot 路径无效: $VcpkgDir"
        exit 1
    }
    Write-Host "[1/4] 复用 vcpkg: $VcpkgDir"
} else {
    $VcpkgDir = Join-Path $FwDir 'vcpkg'
    if (-not (Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake'))) {
        Write-Host "[1/4] vcpkg 未找到, clone 到 $VcpkgDir..."
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

# 检查 OpenSSL 已安装 (lib 名根据 triplet 不同)
$opensslMarker = @(
    (Join-Path $OpensslRoot 'lib\libssl.a'),
    (Join-Path $OpensslRoot 'lib\libssl.lib'),
    (Join-Path $OpensslRoot 'lib\ssl.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $opensslMarker) {
    Write-Host '  安装 vcpkg 依赖...'
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
    Write-Error 'OpenSSL 安装失败, 检查 vcpkg 日志'
    exit 1
}
Write-Host '  vcpkg 依赖就绪'

# ── [2/4] 构建 libhv ──
$libhvLib = @(
    (Join-Path $LibhvInstall 'lib\libhv_static.a'),
    (Join-Path $LibhvInstall 'lib\libhv.a'),
    (Join-Path $LibhvInstall 'lib\hv_static.lib'),
    (Join-Path $LibhvInstall 'lib\hv.lib')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $libhvLib) {
    Write-Host '[2/4] 构建 libhv...'
    & (Join-Path $FwDir 'build-libhv.ps1') -OpensslRoot $OpensslRoot -Generator $Generator
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $libhvLib = @(
        (Join-Path $LibhvInstall 'lib\libhv_static.a'),
        (Join-Path $LibhvInstall 'lib\libhv.a'),
        (Join-Path $LibhvInstall 'lib\hv_static.lib'),
        (Join-Path $LibhvInstall 'lib\hv.lib')
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
} else {
    Write-Host '[2/4] libhv 已有缓存, 跳过'
}
if (-not $libhvLib) { Write-Error 'libhv 构建失败'; exit 1 }
Write-Host "  libhv 就绪: $libhvLib"

# ── [3/4] 构建 fw ──
Write-Host '[3/4] 构建 fw 库...'
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

# 增量 configure
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

# ── [4/4] 安装 / 测试 ──
if ($Test) {
    Write-Host '[4/4] 运行测试...'
    Push-Location $BuildDir
    try {
        & ctest --output-on-failure
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally { Pop-Location }
} else {
    Write-Host "[4/4] 安装到 $InstallDir..."
    & cmake --install $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host ''
Write-Host "安装完成: $InstallDir"
