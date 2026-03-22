# Examples

## Overview

DXBridge v1 includes examples at two levels:

This catalog reflects the `v1.3.0` onboarding refresh layered on top of the stable DXBridge v1 line.

- native C++ examples built by CMake
- language bindings and samples that consume the public DLL ABI directly

The example suites deliberately repeat the same learning path so behavior is easy to compare across runtimes.

## `v1.3.0` release focus

The consumer-facing value of `v1.3.0` is easiest to understand through the examples:

- scenarios 01 through 05 keep the familiar `1.x` flow unchanged
- scenario 06 adds pre-init capability discovery before backend selection
- Bun, Node.js, Python, Go, and Rust now expose aligned flags, helper names, and output goals for that onboarding path
- `examples/run_capability_preflight.ps1` turns the same story into a repeatable cross-runtime smoke run

The scenario numbers are consistent across runtimes even when the final file names differ slightly. In particular, the fifth scenario is the triangle-rendering sample everywhere, but Bun and Node.js use `example05_dx11_triangle.*` while Go, Python, C#, Java, and Rust keep `example05_dx11_moving_triangle.*`.

For folder-level navigation when browsing the repository, see `examples/README.md`.

## Scenario model used across examples

Most language folders cover the same six scenarios:

1. load the DLL, decode version information, and receive logs
2. enumerate adapters using the two-call ABI pattern
3. create a device and inspect thread-local native errors correctly
4. create a real Win32 window and run a DX11 clear/present loop
5. render a triangle through shaders, pipeline state, and vertex buffers
6. preflight backend capability discovery before choosing DX11 vs DX12

That repeated sequence is intentional: it gives each runtime the same progression from simple DLL loading to real rendering.

## Runtime matrix

| Path | Runtime | Binding approach | Best use |
| --- | --- | --- | --- |
| `examples/hello_triangle/` | C++ | direct ABI use in native code | canonical native reference |
| `examples/bun/` | Bun + TypeScript | handwritten Bun FFI | modern JS/TS FFI reference |
| `examples/nodejs/` | Node.js + JavaScript | handwritten `koffi` bindings | plain Node.js integration |
| `examples/go/` | Go | handwritten Windows DLL calls | pure Go without `cgo` |
| `examples/python/` | Python | `ctypes` | lightweight scripting integration |
| `examples/csharp/` | .NET | manual P/Invoke-style loading | managed Windows integration |
| `examples/java/` | Java | JNA / JNA Platform | JVM integration |
| `examples/rust/` | Rust | handwritten FFI + `libloading` | systems-language FFI reference |

## Native examples

### `examples/hello_triangle/`

- `main_dx11.cpp` - Win32 DX11 triangle example with a `--self-test` path used by CTest
- `main_dx12.cpp` - Win32 DX12 triangle example

These examples are the clearest reference for direct ABI use from native code.

Reference guide: `examples/hello_triangle/README.md`

## Bun examples

Location: `examples/bun/`

Purpose of the Bun suite:

- prove that Bun FFI can call the real `dxbridge.dll` without a custom addon
- demonstrate how to encode the ABI structs manually from TypeScript
- provide practical windowed and rendering examples, not just load-time smoke tests

Key files:

- `examples/bun/dxbridge.ts` - handwritten Bun FFI bindings, constants, struct encoders, DLL discovery, helpers
- `examples/bun/win32.ts` - Win32 window creation and message pumping through Bun FFI
- `examples/bun/example01_load_version_logs.ts`
- `examples/bun/example02_enumerate_adapters.ts`
- `examples/bun/example03_create_device_errors.ts`
- `examples/bun/example04_dx11_clear_window.ts`
- `examples/bun/example05_dx11_triangle.ts`
- `examples/bun/example06_capability_preflight.ts`

Important implementation notes:

- opaque DXBridge handles are represented as `bigint`
- struct layout is manually encoded to stay aligned with the 64-bit Windows ABI
- log callback lifetime must be managed explicitly on the Bun side
- DX11 windowed examples require a real `HWND`, not a fake numeric handle
- the Bun suite is self-contained and does not require a separate package install step

Recommended starting command:

```bat
bun run examples/bun/example01_load_version_logs.ts
```

Reference guide: `examples/bun/README.md`

## Node.js examples

Location: `examples/nodejs/`

Purpose of the Node.js suite:

- validate the same ABI from standard Node.js using `koffi`
- show a plain JavaScript path alongside the Bun TypeScript variant
- document dependency-local installation inside the example folder only

Key files:

- `examples/nodejs/package.json` - local package and sample scripts
- `examples/nodejs/dxbridge.js` - handwritten bindings, struct encoders, DLL lookup, helpers
- `examples/nodejs/win32.js` - Win32 helper for real `HWND` creation
- `examples/nodejs/example01_load_version_logs.js`
- `examples/nodejs/example02_enumerate_adapters.js`
- `examples/nodejs/example03_create_device_errors.js`
- `examples/nodejs/example04_dx11_clear_window.js`
- `examples/nodejs/example05_dx11_triangle.js`
- `examples/nodejs/example06_capability_preflight.js`

Important implementation notes:

- the suite depends on local `koffi` installation in `examples/nodejs/`; use `npm ci` for a lockfile-reproducible setup
- opaque handles are represented as `bigint`
- log callback and Win32 procedure callbacks must remain strongly reachable in JavaScript
- encoded structs must stay synchronized with the public header layout

Install and first run:

```bat
npm --prefix examples/nodejs ci
node examples/nodejs/example01_load_version_logs.js
```

Reference guide: `examples/nodejs/README.md`

## Other language example suites

The repository also contains focused READMEs for:

- `examples/go/README.md`
- `examples/python/README.md`
- `examples/csharp/README.md`
- `examples/java/README.md`
- `examples/rust/README.md`

Those suites reinforce the same ABI-first design and make DXBridge easier to adopt from multiple ecosystems.

For `v1.3.0`, the onboarding-focused capability-preflight coverage is currently strongest in Bun, Node.js, Python, Go, and Rust because those bindings carry the most explicit DLL-discovery and struct-encoding responsibilities.

The release-ready example story keeps scenario 06 aligned across those runtimes:

- the same `--backend all|dx11|dx12` flag shape for the pre-init pass
- the same optional `--compare-active dx11|dx12` comparison against legacy `DXBridge_SupportsFeature()`
- a shared PowerShell launcher at `examples/run_capability_preflight.ps1` for quick cross-runtime smoke runs

## Release-oriented first runs

If you only want the fastest `v1.3.0` onboarding checks, start here:

- Bun: `bun run examples/bun/example06_capability_preflight.ts --backend all`
- Node.js: `node examples/nodejs/example06_capability_preflight.js --backend all`
- Python: `python examples/python/example_06_capability_preflight.py --backend all`
- Go: `go run ./cmd/example06_capability_preflight --backend all`
- Rust: `cargo run --manifest-path examples\rust\Cargo.toml --bin example06_capability_preflight -- --backend all`
- Cross-runtime: `pwsh -File examples/run_capability_preflight.ps1 -Runtime all -Backend all`

## Choosing the right example path

- Use `examples/hello_triangle/` if you want the most direct native reference
- Use `examples/bun/` if you want modern TypeScript plus direct FFI
- Use `examples/nodejs/` if you want plain JavaScript plus `koffi`
- Start with the `example06_capability_preflight` variant in each runtime when you need pre-init backend discovery before choosing DX11 vs DX12
- Use `examples/run_capability_preflight.ps1` when you want to validate the same scenario across multiple runtimes from one entry point
- Use the other language folders when validating cross-language portability of the ABI

## Cross-runtime compatibility notes

- all handles are 64-bit and should be treated as opaque tokens
- every public input struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`
- `DXBridge_GetLastError()` is thread-local and should be read immediately after the failing call
- log callbacks must remain strongly reachable in GC-managed runtimes
- DX11 windowed examples require a real Win32 `HWND`; a fake integer value is not sufficient
- `DXBridge_GetBackBuffer()` returns a swap-chain-owned handle and does not have a separate public destroy call
- `DXBridge_QueryCapability()` is the additive pre-init discovery family; `DXBridge_SupportsFeature()` remains active-backend scoped after `DXBridge_Init()`

## Suggested reading order

1. `examples/hello_triangle/README.md` for the native baseline
2. `examples/bun/README.md` or `examples/nodejs/README.md` for JS runtime integration
3. the README for your target runtime under `examples/`
4. `docs/api-reference.md` when implementing or verifying your own binding layer

## Related documentation

- `README.md`
- `docs/build-and-test.md`
- `docs/api-reference.md`
- `examples/README.md`
