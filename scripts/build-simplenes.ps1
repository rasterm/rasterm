param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'apps\SimpleNES'
$appBuild = Join-Path $sourceRoot 'build'
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw 'Set VCPKG_ROOT or pass -VcpkgRoot with a bootstrapped vcpkg installation.'
}
$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (!(Test-Path -LiteralPath $toolchain)) {
    throw "The vcpkg CMake toolchain was not found at '$toolchain'."
}

& $cmake -S $sourceRoot -B $appBuild -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_MANIFEST_DIR=$sourceRoot" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
if ($LASTEXITCODE -ne 0) { throw 'SimpleNES configuration failed.' }

& $cmake --build $appBuild --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'SimpleNES build failed.' }

Write-Host "Built SimpleNES at $(Join-Path $appBuild "$Configuration\SimpleNES.exe")"
