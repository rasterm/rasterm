param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'apps\examples'
$buildRoot = Join-Path $sourceRoot 'build'
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source

& $cmake -S $sourceRoot -B $buildRoot -A x64
if ($LASTEXITCODE -ne 0) { throw 'rasterm examples configuration failed.' }

& $cmake --build $buildRoot --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'rasterm examples build failed.' }

Write-Host "Built rasterm examples at $(Join-Path $buildRoot $Configuration)"
