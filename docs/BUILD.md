# Build and Installation

Run these commands from the rasterm repository root in PowerShell. rasterm 1.1 supports
Windows x64 and C++20. C++ consumers use the static library; language bindings can use
the optional shared C ABI. You need Visual Studio 2022 v143
with the Desktop development with C++ workload, CMake 3.24+, and Windows Terminal 1.22+
(1.23+ recommended).

## rasterm Core

The library has no third party dependency:

```powershell
./scripts/build-rasterm.ps1
```

```powershell
.\scripts\build-rasterm.ps1 -Configuration Debug
.\scripts\build-rasterm.ps1 -Configuration Release
```

By default the script builds and installs only the dependency free static library and
headers in Debug and Release. Use `-Configuration Debug` or `-Configuration Release`
to run only one configuration. Optional work is explicit:

```powershell
./scripts/build-rasterm.ps1 -BuildShared
./scripts/build-rasterm.ps1 -BuildExamples -RunTests -RunBenchmarks
./scripts/build-rasterm.ps1 -Full
```

`-Full` is the maintainer release gate: it enables the shared C ABI, examples, tests,
benchmark smoke test, install verification, and clean C/C++ consumers for both
configurations.

The equivalent minimal manual build is:

```powershell
cmake -S . -B build/core -A x64 -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/core --config Release --parallel
cmake --install build/core --config Release --prefix build/install
```

or:

```powershell
cmake -S . -B build/core -A x64 `
  -DRASTERM_BUILD_TESTS=ON `
  -DRASTERM_BUILD_EXAMPLES=ON `
  -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/core --config Release --parallel
ctest --test-dir build/core -C Release --output-on-failure
cmake --install build/core --config Release --prefix build/install
```

Outputs:

```text
build/core/Release/rasterm.lib
build/core/Release/rasterm-example-*.exe
build/install/include/rasterm/
build/install/lib/rasterm.lib
build/install/lib/cmake/rasterm/
```

For Debug, replace `Release` with `Debug`; the library is `rastermd.lib`. Official MSVC
artifacts use `/MD` in Release and `/MDd` in Debug. Do not mix configurations or CRTs.

## Shared C ABI and Bindings

The dependency free DLL exports only the stable C API and leaves the C++ ABI private:

```powershell
cmake -S . -B build/bindings `
  -DRASTERM_BUILD_SHARED_C_API=ON `
  -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/bindings --config Release --target rasterm-shared
```

Output: `build/bindings/Release/rasterm.dll` and `rasterm-import.lib`. Rust/Python
commands, ownership rules, and package layouts are in [`BINDINGS.md`](BINDINGS.md).

GitHub CI builds only the dependency free engine, examples, validation targets,
benchmarks, and clean installed package consumers. Its Release ZIP contains public
headers, `rasterm.lib`, license notices, and the Rust/Python binding sources.
CMake/pkg-config metadata remains available
from `cmake --install`, but is not included in the minimal binary release. rPlayer,
SimpleNES, RetroArch, and their dependencies are deliberately excluded.

After MSVC, clang-cl, and MinGW64 pass on `master`, GitHub Actions creates or updates the
release for the version in `project(rasterm VERSION ...)` at the top of `CMakeLists.txt`.
That same value controls the ZIP filename, GitHub tag, and release title, so `build.yml`
does not need a version edit. The release contains only the minimal core ZIP described
above. Pull requests and manual runs never publish anything.

## Consume the Installed CMake Package

On a clean machine, copy/install the `build/install` tree, then use only public headers:

```cmake
cmake_minimum_required(VERSION 3.24)
project(example LANGUAGES CXX)
find_package(rasterm CONFIG REQUIRED)
add_executable(example main.cpp)
target_compile_features(example PRIVATE cxx_std_20)
target_link_libraries(example PRIVATE rasterm::rasterm)
```

```powershell
cmake -S C:/path/to/example -B C:/path/to/example/build -A x64 `
  -DCMAKE_PREFIX_PATH=C:/path/to/rasterm/install
cmake --build C:/path/to/example/build --config Release
```

The same installed package supports a C11 target using `<rasterm/capi.h>`; set
`LINKER_LANGUAGE CXX` only if the generator cannot infer the C++ static library linker.
Reference consumers are under `validation/tests/consumer` and
`validation/tests/c_consumer`.

## rPlayer

rPlayer is optional and uses the root `vcpkg.json` manifest for OpenCV, FFmpeg, and
miniaudio. Install [vcpkg](https://github.com/microsoft/vcpkg), set `VCPKG_ROOT`, and use
a Developer PowerShell for Visual Studio:

```powershell
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
msbuild rasterm.sln /m /p:Configuration=Release /p:Platform=x64 `
  /p:VcpkgRoot="$env:VCPKG_ROOT\" /p:VcpkgEnableManifest=true
```

Output: `build/x64/Release/apps/rPlayer/rPlayer.exe`. Put the vcpkg runtime DLLs beside
the executable or run in an environment where `vcpkg_installed/x64-windows/bin` is on
`PATH`. rPlayer metrics are written to `rasterm-metrics.csv` in its working directory.

## SimpleNES

SimpleNES consumes the installed rasterm package and OpenCV from vcpkg:

```powershell
cmake -S apps/SimpleNES -B build/simplenes -A x64 `
  -DCMAKE_PREFIX_PATH="$PWD/build/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/simplenes --config Release --parallel
```

Run an NTSC ROM:

```powershell
build/simplenes/Release/SimpleNES.exe C:/Games/game.nes
```

OpenCV must be built for the same x64 configuration. Details and controls are in
[`apps/SimpleNES/README.md`](../apps/SimpleNES/README.md).

## RetroArch Integration (experimental)

This integration is optional and still experimental. It needs MSYS2 MINGW64, GCC, make,
CMake, pkg-config, ntldd, and RetroArch's normal Windows build dependencies. The included
script builds rasterm with MinGW, installs its pkg-config metadata, configures RetroArch with
`--enable-rasterm --disable-sixel`, builds it, and packages dependent DLLs:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-retroarch.ps1 -Jobs 8
```

Add `-Debug` for RetroArch's `DEBUG=1 GL_DEBUG=1` build. If MSYS2 is not at
`C:\msys64`, set `MSYS2_ROOT` first. Packaged output:

```text
apps/retroarch/dist/rasterm-retroarch/retroarch.exe
apps/retroarch/dist/rasterm-retroarch/rasterm.cfg
```

PowerShell launch example (backticks are line continuations only in PowerShell):

```powershell
cd apps/retroarch/dist/rasterm-retroarch
.\retroarch.exe --verbose --config .\rasterm.cfg `
  -L .\cores\core_libretro.dll .\games\game.rom
```

In `cmd.exe`, enter the command on one line and do not include backticks.

## MinGW64 Core

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
cmake -S . -B build/mingw -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/mingw --parallel
ctest --test-dir build/mingw --output-on-failure
```

Package `libwinpthread-1.dll` and `libgcc_s_seh-1.dll` from the same MSYS2 toolchain
beside MinGW executables. An unrelated DLL of the same name earlier on `PATH` can cause
startup failure `0xc0000139`.

## Benchmarks, Sanitizers, Fuzzers, and Archives

```powershell
cmake -S . -B build/bench -A x64 -DRASTERM_BUILD_BENCHMARKS=ON
cmake --build build/bench --config Release --parallel
build/bench/Release/rasterm-encoder-benchmark.exe

cmake -S . -B build/asan -A x64 -DRASTERM_ENABLE_ASAN=ON
cmake --build build/asan --config Debug --parallel
ctest --test-dir build/asan -C Debug --output-on-failure

cpack --config build/core/CPackConfig.cmake -C Release -G ZIP
cpack --config build/core/CPackSourceConfig.cmake -G ZIP
```

Fuzz targets require Clang with libFuzzer and `RASTERM_BUILD_FUZZERS=ON`. Release
dependencies are pinned by the vcpkg baseline in `vcpkg.json`.
