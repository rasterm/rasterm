# Tests

The test groups cover the C++ engine, stable C ABI, SIXEL encoding and parsing,
frame validation and differencing, palettes, terminal output, and long running
stress behavior. `c_consumer/` additionally verifies that an installed rasterm
package can be consumed by a pure C project.

`rasterm.stress` defaults to 2,000 rendered frames plus 10,000 presenter submissions. Set `RASTERM_STRESS_FRAMES` to a larger value for soak runs without slowing normal CI.