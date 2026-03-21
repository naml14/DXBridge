# Architecture

## Overview

DXBridge v1 is built around a stable C ABI on top of backend-specific C++ implementations. The external contract is intentionally narrow and portable across consumers, while internal code remains organized by rendering backend.

```text
Consumer code
    |
    v
DXBridge_* exported C ABI
    |
    v
IBackend dispatch layer
    |
    +--> DX11Backend
    |
    \--> DX12Backend
```

This document is the internal companion to `docs/api-reference.md`: the API reference explains what consumers call, while this page explains how the repository is organized to support that ABI.

## Design goals

DXBridge v1 optimizes for four things:

- a small, stable C ABI that is easy to consume from multiple runtimes
- backend interchangeability behind a single exported surface
- defensive resource handling through opaque handles instead of exposed COM pointers
- practical validation through both tests and real example programs

## Scope of v1

Version 1 focuses on foundational rendering and interop workflows:

- process-level backend initialization
- adapter discovery and device creation
- windowed swap-chain presentation
- shader, pipeline, buffer, and render-target creation
- frame submission with explicit begin/end boundaries that work across DX11 and DX12
- public escape hatches for advanced native integrations

## Core components

### Public ABI

- `include/dxbridge/dxbridge.h` defines all public handles, enums, structs, and exported functions
- `include/dxbridge/dxbridge_version.h` defines the semantic version and `DXBRIDGE_STRUCT_VERSION`
- `src/dxbridge.cpp` implements the exported entry points and forwards calls to the active backend

Important public design choices:

- all opaque resources are 64-bit integer handles
- every ABI struct carries `struct_version` at offset 0
- exported functions return `DXBResult` and leave detailed error text in thread-local storage
- versioning is explicit in headers so bindings can validate layout expectations at runtime

### Shared runtime infrastructure

`src/common/` holds the pieces shared by both backends:

- `handle_table.hpp` - generational handle pools with type tagging and stale-handle protection
- `error.cpp` / `error.hpp` - HRESULT mapping and thread-local error text
- `log.cpp` / `log.hpp` - process-wide log callback dispatch
- `backend.hpp` - `IBackend` contract implemented by DX11 and DX12
- `format_util.hpp` - translation helpers between DXBridge enums and DXGI/D3D formats

### Backend implementations

- `src/dx11/` contains the Direct3D 11 backend
- `src/dx12/` contains the Direct3D 12 backend

Both backends expose the same functional surface through `IBackend`, including:

- adapter enumeration
- device creation
- swap-chain management
- render target and depth-stencil creation
- shader compilation
- input layout and pipeline state creation
- buffer upload and render commands

That shared contract is what allows `src/dxbridge.cpp` to stay thin: it validates the public ABI boundary, then forwards calls to the active backend implementation.

## Backend selection model

`DXBridge_Init()` owns backend activation.

- `DXB_BACKEND_AUTO` attempts DX12 first, then falls back to DX11
- `DXB_BACKEND_DX12` requires DX12 availability
- `DXB_BACKEND_DX11` selects the DX11 backend directly

After initialization, `src/dxbridge.cpp` stores the active backend in the global `dxb::g_backend` pointer and routes all public API calls through it.

Practical implication for consumers: backend choice is process-wide in v1. The API does not expose mixed DX11 and DX12 devices in the same process through separate DXBridge instances.

## Handle model

DXBridge does not expose native COM pointers directly. Instead it uses encoded handles whose fields include:

- object type tag
- API version byte
- generation counter
- slot index

This allows v1 to:

- detect invalid or stale handles after destruction
- keep the ABI language-friendly
- maintain backend-specific native objects internally

This handle model is one of the main reasons the API ports cleanly to Bun, Node.js, Go, Python, C#, Java, and Rust without a generated wrapper layer.

The logic lives in `src/common/handle_table.hpp`.

## Resource lifetime model

The lifetime rules are intentionally explicit:

- devices own backend state and are the root of most child objects
- swap chains own their back-buffer resources
- `DXBRenderTarget` handles returned by `DXBridge_GetBackBuffer()` are swap-chain-owned and are not destroyed directly
- callers are expected to destroy child objects before destroying the parent device or swap chain

The public API keeps destroy functions narrow and predictable, while the implementation uses handle generations to reject stale references.

## Error and logging model

### Errors

- operations return a compact `DXBResult`
- detailed text is retrieved with `DXBridge_GetLastError()`
- the last native HRESULT is retrieved with `DXBridge_GetLastHRESULT()`
- error state is thread-local, so callers must read it immediately after a failing call on the same thread

This is especially important for binding authors: any additional native call on the same thread can overwrite the detailed message or HRESULT you wanted to inspect.

### Logs

- consumers can install a process-wide callback via `DXBridge_SetLogCallback()`
- backends and shared code use internal log helpers to publish informational, warning, and error messages
- runtime bindings must keep callback function pointers alive for as long as native code may invoke them

Because callback registration is process-wide, the examples intentionally keep one active callback at a time per runtime.

## Frame model

DXBridge deliberately exposes a portable frame flow even though DX11 and DX12 have different internal expectations.

Typical sequence:

1. `DXBridge_BeginFrame(device)`
2. bind render targets and viewport
3. clear and bind pipeline state
4. bind buffers and draw
5. `DXBridge_EndFrame(device)`
6. `DXBridge_Present(swapchain, sync_interval)`

Backend-specific behavior:

- DX11 treats begin/end frame calls mostly as validated no-ops
- DX12 uses begin/end frame as real lifecycle boundaries
- DX12 rejects presentation or resize operations while a frame is still active

## Testing strategy in v1

The v1 test suite intentionally mixes internal validation and end-to-end checks.

- unit tests validate handle-table behavior, HRESULT mapping, and backend-side input validation
- backend validation suites cover DX11 and DX12 failure paths and defensive behavior
- integration tests render headless triangles and validate readback results
- the DX11 C++ example includes a self-test mode wired into CTest smoke coverage

The result is a layered confidence model: low-level validation catches ABI/runtime issues early, while example-driven smoke coverage confirms that the published usage path still works.

## Example strategy

Examples are not limited to the native C++ flow. The repository treats language interop as a first-class deliverable.

- C++ examples validate the direct ABI usage model
- Bun and Node.js examples prove that the DLL can be consumed from JavaScript runtimes without a custom native addon
- other language folders repeat the same usage patterns so the ABI remains easy to port

This is one of the key release themes of v1: the API is designed for both rendering experiments and cross-language integration.

## Glossary

| Term | Meaning in DXBridge |
| --- | --- |
| ABI | The stable exported C contract defined in `include/dxbridge/dxbridge.h` |
| Backend | The active renderer implementation: DX11 or DX12 |
| Handle | Opaque 64-bit token representing an internal object |
| RTV | Render target view created from a render target or swap-chain back buffer |
| DSV | Depth-stencil view and its backing depth resource |
| Native escape hatch | `DXBridge_GetNativeDevice()` or `DXBridge_GetNativeContext()` |

## Related documentation

- `README.md`
- `docs/api-reference.md`
- `docs/build-and-test.md`
- `docs/examples.md`
