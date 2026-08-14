# Core Validation Tests

The isolated test executables belong to the unified `validation/CMakeLists.txt` kit. They cover
the C++ engine, stable C ABI, installed header self containment, scaling, SIXEL encoding and
parsing, frame validation and differencing, palettes, terminal output, and long running stress
behavior. `consumer/` and `c_consumer/` verify that an installed rasterm package can be consumed
by clean C++ and C projects.

`rasterm.stress` defaults to 2,000 rendered frames plus 10,000 presenter submissions. Set `RASTERM_STRESS_FRAMES` to a larger value for soak runs without slowing normal CI.
