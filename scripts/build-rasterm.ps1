param(
    [ValidateSet('Both', 'Debug', 'Release')]
    [string]$Configuration = 'Both',
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'build\rasterm'
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source

& $cmake -S $repoRoot -B $buildRoot -A x64 `
    -DRASTERM_BUILD_EXAMPLES=OFF `
    -DRASTERM_BUILD_TESTS=OFF `
    -DRASTERM_BUILD_BENCHMARKS=OFF `
    -DRASTERM_BUILD_SHARED_C_API=ON `
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

    if (!$SkipTests) {
        & $cmake -E env CTEST_OUTPUT_ON_FAILURE=1 `
            ctest --test-dir $buildRoot -C $current --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "rasterm $current tests failed." }

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

    $installRoot = Join-Path $repoRoot "build\rasterm-install\$current"
    & $cmake --install $buildRoot --config $current --prefix $installRoot
    if ($LASTEXITCODE -ne 0) { throw "rasterm $current installation failed." }
    & $cmake "-DROOT=$installRoot" -DINSTALL_TREE=ON `
        -P (Join-Path $repoRoot 'cmake\VerifyReleaseContents.cmake')
    if ($LASTEXITCODE -ne 0) { throw "rasterm $current install verification failed." }

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

    Write-Host "Validated rasterm $current at $(Join-Path $buildRoot $current)"
}
