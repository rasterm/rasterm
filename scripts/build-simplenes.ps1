param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$coreBuild = Join-Path $repoRoot 'build\simplenes-core'
$installRoot = Join-Path $repoRoot 'build\simplenes-install'
$appBuild = Join-Path $repoRoot 'build\simplenes'
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw 'Set VCPKG_ROOT or pass -VcpkgRoot with a bootstrapped vcpkg installation.'
}
$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (!(Test-Path -LiteralPath $toolchain)) {
    throw "The vcpkg CMake toolchain was not found at '$toolchain'."
}

& $cmake -S $repoRoot -B $coreBuild `
    -DRASTERM_BUILD_TESTS=OFF `
    -DRASTERM_WARNINGS_AS_ERRORS=ON
if ($LASTEXITCODE -ne 0) { throw 'rasterm core configuration failed.' }

& $cmake --build $coreBuild --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'rasterm core build failed.' }

& $cmake --install $coreBuild --config $Configuration --prefix $installRoot
if ($LASTEXITCODE -ne 0) { throw 'rasterm core installation failed.' }

& $cmake -S (Join-Path $repoRoot 'apps\SimpleNES') -B $appBuild `
    "-DCMAKE_PREFIX_PATH=$installRoot" `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_MANIFEST_DIR=$repoRoot" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
if ($LASTEXITCODE -ne 0) { throw 'SimpleNES configuration failed.' }

& $cmake --build $appBuild --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'SimpleNES build failed.' }

Write-Host "Built SimpleNES at $(Join-Path $appBuild "$Configuration\SimpleNES.exe")"
