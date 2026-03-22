# Examples

## Overview

This folder contains the publication-ready DXBridge v1 example suites with the `v1.3.0` onboarding refresh. Each language folder documents the same core learning path so behavior is easy to compare across runtimes.

The shared progression is:

1. load `dxbridge.dll` and decode the version
2. enumerate adapters
3. create a device and inspect errors correctly
4. create a real Win32 window and clear it through DX11
5. render a triangle through shaders, pipeline state, and buffers
6. preflight backend capability discovery before choosing DX11 or DX12

Scenario 06 is the productized `v1.3.0` addition. Bun, Node.js, Python, Go, and Rust now expose the same capability-preflight story with matching flags and output goals:

- `--backend all|dx11|dx12` to focus the pre-init pass
- `--compare-active dx11|dx12` to contrast the new pre-init query flow with legacy `DXBridge_SupportsFeature()` behavior
- optional `--dll PATH` overrides everywhere

For a quick cross-runtime smoke path on Windows PowerShell, use `examples/run_capability_preflight.ps1`.

The important release message is simple: scenarios 01 through 05 still describe the stable `1.x` rendering flow, while scenario 06 is the new optional onboarding path for backend and validation preflight.

## Folder map

| Path | Runtime | Guide |
| --- | --- | --- |
| `examples/hello_triangle/` | Native C++ | [`hello_triangle/README.md`](hello_triangle/README.md) |
| `examples/bun/` | Bun + TypeScript | [`bun/README.md`](bun/README.md) |
| `examples/nodejs/` | Node.js + JavaScript | [`nodejs/README.md`](nodejs/README.md) |
| `examples/go/` | Go | [`go/README.md`](go/README.md) |
| `examples/python/` | Python | [`python/README.md`](python/README.md) |
| `examples/csharp/` | C# / .NET | [`csharp/README.md`](csharp/README.md) |
| `examples/java/` | Java | [`java/README.md`](java/README.md) |
| `examples/rust/` | Rust | [`rust/README.md`](rust/README.md) |

## Choosing a starting point

- Start with scenario 06 when you need to pick a backend before calling `DXBridge_Init()`.
- Start with [`hello_triangle/README.md`](hello_triangle/README.md) for the most direct native reference.
- Use [`bun/README.md`](bun/README.md) or [`nodejs/README.md`](nodejs/README.md) when implementing a JavaScript or TypeScript binding.
- Use the runtime-specific README that matches your target language for setup details, command patterns, and validation steps.

## Related documentation

- [`../README.md`](../README.md)
- [`../docs/examples.md`](../docs/examples.md)
- [`../docs/api-reference.md`](../docs/api-reference.md)
