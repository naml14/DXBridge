# DXBridge

DXBridge is a Windows-native Direct3D bridge distributed as `dxbridge.dll`. It exposes a stable C ABI so native code and higher-level runtimes can drive Direct3D 11 and Direct3D 12 through one handle-based API.

Version `1.0.1` is the current publication-ready patch release of the project. It preserves the documented v1 ABI while aligning the public release metadata, docs, and example guidance after the initial `1.0.0` publication.

For consumers, that means the declared C ABI/API surface remains the same v1 contract from `1.0.0`; `1.0.1` focuses on synchronization, reproducibility guidance, and release-facing cleanup.

## License and usage restrictions

DXBridge is distributed under a custom proprietary license in `LICENSE`.

This repository is not open source. Any permitted use requires visible credit
to the author or rights holder. Any use of the repository, project, code,
examples, documentation, or binaries in another project also requires both
prior notification and prior written authorization from the rights holder.

Required notice must be sent through at least one GitHub-related contact method
of the rights holder associated with this repository and additionally to
`naml@electrondevnaml.xyz`.

Important: visible credit and notification do not replace prior written
authorization. Without written authorization, reuse in another project is
prohibited.

See `LICENSE` and `docs/license-policy.md` before reusing anything from this
repository.

## At a glance

| Topic | Value |
| --- | --- |
| Public headers | `include/dxbridge/dxbridge.h`, `include/dxbridge/dxbridge_version.h` |
| Current version | `1.0.1` |
| Language standard | C++17 |
| Backends | Direct3D 11 and Direct3D 12 |
| Platform | Windows 10/11 |
| Build system | CMake presets for Visual Studio 2022 |
| Test runner | CTest |
| Example coverage | C++, Bun, Node.js, Go, Python, C#, Java, Rust |

## Why DXBridge exists

DXBridge provides an ABI-first layer for rendering experiments, tools, and language interop where linking directly against C++ renderer code is not desirable. The published contract stays in C while the implementation remains split into backend-specific C++ modules.

The v1 surface includes:

- backend initialization and capability detection
- adapter enumeration and device creation
- swap-chain creation and presentation
- buffers, shaders, input layouts, and pipeline state
- frame submission with draw and draw-indexed commands
- thread-local error retrieval and process-wide log callbacks
- native and multi-language examples that exercise the real DLL

## Quick start

From the repository root:

```bat
cmake --preset debug
cmake --build --preset debug
pwsh -File scripts\run-ctest.ps1 -Preset debug
```

Then try one of the documented sample paths:

```bat
out\build\debug\Debug\hello_triangle.exe --self-test
bun run examples/bun/example01_load_version_logs.ts
node examples/nodejs/example01_load_version_logs.js
```

For fuller setup and validation commands, see `docs/build-and-test.md` and `docs/examples.md`.

## Compatibility and assumptions

- Windows 10 or 11
- Visual Studio 2022 with the MSVC C++ toolchain
- CMake `3.20+`
- Windows SDK `10.x`
- A normal Windows desktop session for windowed examples

Source-level compatibility notes:

- minimum Windows SDK noted in `include/dxbridge/dxbridge.h`: `10.0.22000.0`
- minimum OS noted in `include/dxbridge/dxbridge.h`: Windows 10 1809

## Repository map

```text
include/dxbridge/    Public ABI headers and version macros
src/common/          Shared infrastructure: errors, logging, format helpers, handle tables
src/dx11/            Direct3D 11 backend implementation
src/dx12/            Direct3D 12 backend implementation
src/dxbridge.cpp     Exported C dispatch layer over the active backend
tests/               Unit, hardening, smoke, and integration coverage
examples/            Native and cross-language usage examples
docs/                Architecture, API, build/test, and example guides
cmake/               Shared CMake helpers and Windows SDK detection
```

## Architecture snapshot

DXBridge is organized in three layers:

1. Public ABI layer: exported `DXBridge_*` functions in `src/dxbridge.cpp`
2. Shared runtime layer: error handling, log dispatch, format helpers, and generational handle pools in `src/common/`
3. Backend layer: `DX11Backend` and `DX12Backend`, both implementing the shared `IBackend` interface

Key design traits in v1:

- opaque 64-bit handles with type tags and generation counters for stale-handle detection
- thread-local native error state exposed through `DXBridge_GetLastError()` and `DXBridge_GetLastHRESULT()`
- runtime backend selection with DX12-first auto mode and DX11 fallback
- explicit native escape hatches via `DXBridge_GetNativeDevice()` and `DXBridge_GetNativeContext()`

Read `docs/architecture.md` for the full design walkthrough.

## Documentation map

| Document | Purpose |
| --- | --- |
| `docs/architecture.md` | Internal design, resource model, and backend structure |
| `docs/api-reference.md` | Complete public ABI reference and compatibility notes |
| `docs/build-and-test.md` | Requirements, presets, build commands, validation, troubleshooting |
| `docs/examples.md` | Example catalog, scenario map, and runtime guidance |
| `docs/license-policy.md` | Plain-language summary of the proprietary usage restrictions |
| `examples/README.md` | Folder-level index for browsing examples from GitHub or a file explorer |
| `CHANGELOG.md` | Release notes and version history |

## Example map

| Path | Focus |
| --- | --- |
| `examples/hello_triangle/` | Native Win32 reference examples for DX11 and DX12 |
| `examples/bun/` | Bun TypeScript FFI bindings and examples |
| `examples/nodejs/` | Node.js JavaScript examples using `koffi` |
| `examples/go/` | Pure Go examples without `cgo` |
| `examples/python/` | Python `ctypes` examples |
| `examples/csharp/` | .NET P/Invoke-style bindings and examples |
| `examples/java/` | Java JNA-based examples |
| `examples/rust/` | Rust handwritten FFI examples |

Every example family follows the same core learning path:

1. load the DLL and decode the version
2. enumerate adapters
3. create a device and inspect errors correctly
4. create a real Win32 window and clear it through DX11
5. render a simple triangle through shaders, pipeline state, and buffers

## Testing at a glance

The CTest suite includes:

- unit validation for handle tables, error mapping, and backend guardrails
- DX11 and DX12 validation regressions
- headless integration triangles for both backends
- a DX11 example self-test wired into the smoke subset

Quick smoke coverage:

```bat
pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke
cmake --build --preset debug --target check_smoke
```

See `tests/README.md` for label guidance.

## Release status

`CHANGELOG.md` now tracks `1.0.1` as the current patch release on top of the initial `1.0.0` baseline. Future updates should preserve the same repository-level structure so the docs stay stable for consumers and contributors.

## Release automation

This repository uses Release Please to keep release PRs, tags, and the changelog aligned with Conventional Commits.

Current release automation files:

- `.github/workflows/releases.yml` runs `googleapis/release-please-action@v4` on pushes to `master`
- `release-please-config.json` defines the root package release strategy
- `.release-please-manifest.json` stores the last released version for the root package
- `version.txt` is the canonical version file consumed by the `simple` release strategy

Version synchronization rules:

- `version.txt` is the source of truth used by Release Please
- `CHANGELOG.md` is updated automatically when a release PR is created
- `CMakeLists.txt` is kept in sync through the `x-release-please-version` annotation on the project version line
- `include/dxbridge/dxbridge_version.h` is kept in sync through the `x-release-please-major`, `x-release-please-minor`, and `x-release-please-patch` annotations
- `scripts/verify-version.ps1` can re-check the synchronized metadata and a built `dxbridge.dll` before or after a release cut

Expected workflow:

1. Merge changes into `master` using Conventional Commits such as `fix:`, `feat:`, or `feat!:`.
2. Release Please updates or opens a release PR.
3. Merge that release PR when you want to publish the next version.
4. Release Please creates the GitHub release, updates `CHANGELOG.md`, records the new version in the manifest, and rewrites the final release notes with AI.

Token note:

- The current workflow uses `${{ github.token }}`.
- If you need CI workflows to run on the release PRs created by Release Please, replace it with a dedicated PAT secret such as `RELEASE_PLEASE_TOKEN`.
- AI release notes currently use `GEMINI_API_KEY` and optional `GEMINI_MODEL`. If the API key is not configured, the workflow falls back to the raw notes generated by Release Please.
