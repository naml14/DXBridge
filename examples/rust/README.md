# Rust examples for `dxbridge.dll`

## Overview

This suite uses plain Rust on Windows with handwritten FFI. It mirrors the same five-step progression used across the DXBridge v1 example families.

The implementation stays intentionally self-contained:

- no generated bindings
- no import library required
- `libloading` resolves exports from `dxbridge.dll` at runtime
- `windows-sys` provides the minimal Win32 pieces for a real `HWND`

## Included examples

- `examples/rust/Cargo.toml` - standalone Cargo package for the samples
- `examples/rust/src/dxbridge.rs` - handwritten ABI structs, DLL loading, helpers, and error handling
- `examples/rust/src/win32.rs` - tiny Win32 helper for creating a real `HWND`
- `examples/rust/src/cli.rs` - small argument parser shared by the examples
- `examples/rust/src/bin/example01_load_version_logs.rs` - DLL loading, version decoding, and log callback lifetime
- `examples/rust/src/bin/example02_enumerate_adapters.rs` - adapter enumeration
- `examples/rust/src/bin/example03_create_device_errors.rs` - immediate error retrieval and successful device creation
- `examples/rust/src/bin/example04_dx11_clear_window.rs` - real Win32 window creation plus a DX11 clear/present loop
- `examples/rust/src/bin/example05_dx11_moving_triangle.rs` - real window, shaders, pipeline state, and animated triangle rendering
- `examples/rust/build.bat` - builds all samples
- `examples/rust/run_example.bat` - runs one sample by binary name

## Requirements

- Windows 10 or 11
- Rust stable toolchain on `PATH`
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- `examples/rust/dxbridge.dll`
- the current working directory
- the running executable directory
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```bat
cargo run --manifest-path examples\rust\Cargo.toml --bin example01_load_version_logs -- --dll D:\trabajo\TestDLL\proyect\out\build\debug\Debug\dxbridge.dll
```

## How to run

From `examples/rust`:

```bat
build.bat
run_example.bat example01_load_version_logs
run_example.bat example02_enumerate_adapters
run_example.bat example03_create_device_errors --debug
run_example.bat example04_dx11_clear_window --hidden --frames 3
run_example.bat example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
```

You can also run the commands directly from `examples/rust`:

```bat
cargo run --bin example01_load_version_logs
cargo run --bin example02_enumerate_adapters
cargo run --bin example03_create_device_errors -- --backend dx11
cargo run --bin example04_dx11_clear_window -- --hidden --frames 3
cargo run --bin example05_dx11_moving_triangle -- --hidden --frames 3 --sync-interval 0
```

## Notes

- Every ABI struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failure on the same thread.
- The DLL log callback is process-wide in these samples, so the helper keeps a single active Rust closure at a time.
- DX11 swap chains need a real Win32 `HWND`. A fake integer handle is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.
- This suite is intentionally 64-bit Windows only, matching the practical setup used by the rest of the repository examples.

## Validation

Recommended local checks:

```bat
cargo build --manifest-path examples\rust\Cargo.toml
cargo run --manifest-path examples\rust\Cargo.toml --bin example01_load_version_logs
cargo run --manifest-path examples\rust\Cargo.toml --bin example02_enumerate_adapters
cargo run --manifest-path examples\rust\Cargo.toml --bin example03_create_device_errors
cargo run --manifest-path examples\rust\Cargo.toml --bin example04_dx11_clear_window -- --hidden --frames 3
cargo run --manifest-path examples\rust\Cargo.toml --bin example05_dx11_moving_triangle -- --hidden --frames 3 --sync-interval 0
```

## Troubleshooting

If the DX11 windowed examples fail, the most likely causes are:

- the DLL was not built yet
- the process is not running in a normal Windows desktop session
- the requested backend or adapter is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine

## Related documentation

- [`../README.md`](../README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
