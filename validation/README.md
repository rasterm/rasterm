# Validation

`validation/CMakeLists.txt` is the single build entry point for correctness tests, installed
header checks, fuzzers, benchmarks, ABI baselines, and repository policy checks. The root project
only selects the kit through `RASTERM_BUILD_TESTS`, `RASTERM_BUILD_FUZZERS`, and
`RASTERM_BUILD_BENCHMARKS`.

`tests/` contains correctness, ABI, installed consumer, stress, and fuzz coverage.
`benchmarks/` contains deterministic workloads and versioned performance baselines.

Tests remain separate executables where process isolation, per test timeouts, allocation failure
hooks, or ABI inspection matter. Shared target creation and include policy live in one helper so
adding a test does not duplicate CMake boilerplate.

Correctness gates run with `RASTERM_BUILD_TESTS=ON`. Performance targets are opt in with
`RASTERM_BUILD_BENCHMARKS=ON`, benchmark changes never replace golden or parser tests.
