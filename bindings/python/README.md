# rasterm for Python

This package is a dependency free, typed wrapper over rasterm's stable C ABI. It accepts
any C contiguous Python buffer, including `bytes`, `bytearray`, `memoryview`, and
C contiguous NumPy arrays without requiring NumPy.

Build the optional shared C ABI and point the package at it:

```powershell
cmake -S ../.. -B ../../build/bindings -DRASTERM_BUILD_SHARED_C_API=ON
cmake --build ../../build/bindings --config Release --target rasterm-shared
$env:RASTERM_LIBRARY = Resolve-Path ../../build/bindings/Release/rasterm.dll
python -m pip install -e . / python -m pip install rasterm
python examples/gradient.py
```

`Engine` borrows a frame only during `render`. `Presenter.submit` is also safe with a
temporary buffer because the C ABI copies the complete submission before returning.
 
