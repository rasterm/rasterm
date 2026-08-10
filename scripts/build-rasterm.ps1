param(
    [ValidateSet('Both', 'Debug', 'Release')]
    [string]$Configuration = 'Both',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [switch]$BuildShared,
    [switch]$BuildExamples,
    [switch]$RunTests,
    [switch]$RunBenchmarks,
    [switch]$VerifyConsumers,
    [switch]$Full
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'build\rasterm'
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source

if ($Full) {
    $BuildShared = $true
    $BuildExamples = $true
    $RunTests = $true
    $RunBenchmarks = $true
    $VerifyConsumers = $true
}

$sharedValue = if ($BuildShared) { 'ON' } else { 'OFF' }
$examplesValue = if ($BuildExamples) { 'ON' } else { 'OFF' }
$testsValue = if ($RunTests) { 'ON' } else { 'OFF' }
$benchmarksValue = if ($RunBenchmarks) { 'ON' } else { 'OFF' }

& $cmake -S $repoRoot -B $buildRoot -A x64 `
    "-DRASTERM_BUILD_EXAMPLES=$examplesValue" `
    "-DRASTERM_BUILD_TESTS=$testsValue" `
    "-DRASTERM_BUILD_BENCHMARKS=$benchmarksValue" `
    "-DRASTERM_BUILD_SHARED_C_API=$sharedValue" `
    "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false" `
    -DRASTERM_WARNINGS_AS_ERRORS=ON
if ($LASTEXITCODE -ne 0) { throw 'rasterm configuration failed.' }

$configurations = if ($Configuration -eq 'Both') {
    @('Debug', 'Release')
}
else {
    @($Configuration)
}

foreach ($current in $configurations) {
    Write-Host "Building rasterm $current..."
    & $cmake --build $buildRoot --config $current --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "rasterm $current build failed." }

    if ($RunTests) {
        & $cmake -E env CTEST_OUTPUT_ON_FAILURE=1 `
            ctest --test-dir $buildRoot -C $current --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "rasterm $current tests failed." }
    }

    if ($RunBenchmarks) {
        $env:RASTERM_BENCH_ITERATIONS = '5'
        try {
            & (Join-Path $buildRoot "$current\rasterm-encoder-benchmark.exe") | Out-Null
            $benchmarkExitCode = $LASTEXITCODE
        }
        finally {
            Remove-Item Env:RASTERM_BENCH_ITERATIONS -ErrorAction SilentlyContinue
        }
        if ($benchmarkExitCode -ne 0) { throw "rasterm $current benchmark smoke test failed." }
    }

    $installKind = if ($BuildShared) { 'shared' } else { 'static' }
    $installRoot = Join-Path $repoRoot "build\rasterm-install\$installKind\$current"
    & $cmake --install $buildRoot --config $current --prefix $installRoot
    if ($LASTEXITCODE -ne 0) { throw "rasterm $current installation failed." }
    & $cmake "-DROOT=$installRoot" -DINSTALL_TREE=ON `
        -P (Join-Path $repoRoot 'cmake\VerifyReleaseContents.cmake')
    if ($LASTEXITCODE -ne 0) { throw "rasterm $current install verification failed." }

    if ($VerifyConsumers) {
        foreach ($consumer in @('consumer', 'c_consumer')) {
            $consumerSource = Join-Path $repoRoot "validation\tests\$consumer"
            $consumerBuild = Join-Path $repoRoot "build\rasterm-consumers\$current\$consumer"
            & $cmake -S $consumerSource -B $consumerBuild -A x64 `
                "-DCMAKE_PREFIX_PATH=$installRoot"
            if ($LASTEXITCODE -ne 0) { throw "$consumer $current configuration failed." }
            & $cmake --build $consumerBuild --config $current --parallel $Jobs
            if ($LASTEXITCODE -ne 0) { throw "$consumer $current build failed." }

            $executable = if ($consumer -eq 'consumer') {
                'rasterm-consumer.exe'
            }
            else {
                'rasterm-c-consumer.exe'
            }
            & (Join-Path $consumerBuild "$current\$executable")
            if ($LASTEXITCODE -ne 0) { throw "$consumer $current smoke test failed." }
        }
    }

    Write-Host "Built and installed rasterm $current at $installRoot"
}
