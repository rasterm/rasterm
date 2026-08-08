# Validation

`tests/` contains correctness, ABI, installed-consumer, stress, and fuzz coverage.
`benchmarks/` contains deterministic workloads and versioned performance baselines.

Correctness gates run with `RASTERM_BUILD_TESTS=ON`. Performance targets are opt-in with
`RASTERM_BUILD_BENCHMARKS=ON`; benchmark changes never replace golden or parser tests.
