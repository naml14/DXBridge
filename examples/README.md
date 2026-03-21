# Examples

## Overview

This folder contains the publication-ready DXBridge v1 example suites. Each language folder documents the same core learning path so behavior is easy to compare across runtimes.

The shared progression is:

1. load `dxbridge.dll` and decode the version
2. enumerate adapters
3. create a device and inspect errors correctly
4. create a real Win32 window and clear it through DX11
5. render a triangle through shaders, pipeline state, and buffers

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

- Start with [`hello_triangle/README.md`](hello_triangle/README.md) for the most direct native reference.
- Use [`bun/README.md`](bun/README.md) or [`nodejs/README.md`](nodejs/README.md) when implementing a JavaScript or TypeScript binding.
- Use the runtime-specific README that matches your target language for setup details, command patterns, and validation steps.

## Related documentation

- [`../README.md`](../README.md)
- [`../docs/examples.md`](../docs/examples.md)
- [`../docs/api-reference.md`](../docs/api-reference.md)
