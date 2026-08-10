param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw 'Set VCPKG_ROOT or pass -VcpkgRoot with a bootstrapped vcpkg installation.'
}
$vcpkg = Join-Path $VcpkgRoot 'vcpkg.exe'
if (!(Test-Path -LiteralPath $vcpkg)) {
    throw "vcpkg.exe was not found at '$vcpkg'. Bootstrap that checkout first."
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if ($msbuild) {
    $msbuildPath = $msbuild.Source
}
else {
    if (!(Test-Path -LiteralPath $vswhere)) {
        throw 'MSBuild was not found in PATH and vswhere.exe is unavailable.'
    }
    $msbuildPath = (& $vswhere -latest -products * `
        -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
        Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($msbuildPath)) {
        throw 'Visual Studio 2022 MSBuild was not found.'
    }
}

$vcpkgRootProperty = $VcpkgRoot.TrimEnd('\') + '\'
& $msbuildPath (Join-Path $repoRoot 'rasterm.sln') `
    "/m:$Jobs" `
    '/t:rPlayer' `
    "/p:Configuration=$Configuration" `
    '/p:Platform=x64' `
    "/p:VcpkgRoot=$vcpkgRootProperty" `
    '/p:VcpkgEnableManifest=true'
if ($LASTEXITCODE -ne 0) { throw 'rPlayer build failed.' }

$output = Join-Path $repoRoot "build\x64\$Configuration\apps\rPlayer\rPlayer.exe"
$installedRoot = Join-Path $repoRoot 'vcpkg_installed\x64-windows\x64-windows'
$installedBin = if ($Configuration -eq 'Debug') {
    Join-Path $installedRoot 'debug\bin'
}
else {
    Join-Path $installedRoot 'bin'
}
$appLocal = Join-Path $VcpkgRoot 'scripts\buildsystems\msbuild\applocal.ps1'
$windowsPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (!$dumpbin -and (Test-Path -LiteralPath $vswhere)) {
    $visualStudio = & $vswhere -latest -products * -property installationPath
    $dumpbin = Get-ChildItem "$visualStudio\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" |
        Sort-Object FullName -Descending |
        Select-Object -First 1
}
if (!$dumpbin) { throw 'dumpbin.exe is required to deploy rPlayer runtime DLLs.' }
$dumpbinPath = if ($dumpbin -is [System.IO.FileInfo]) { $dumpbin.FullName } else { $dumpbin.Source }

$originalPath = $env:PATH
try {
    $env:PATH = "$(Split-Path $dumpbinPath);$originalPath"
    & $windowsPowerShell -NoProfile -ExecutionPolicy Bypass -File $appLocal $output $installedBin
}
finally {
    $env:PATH = $originalPath
}
if (@(Get-ChildItem (Split-Path $output) -Filter '*.dll').Count -eq 0) {
    throw 'rPlayer runtime deployment produced no DLLs.'
}

Write-Host "Built rPlayer at $output"
