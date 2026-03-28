# API Reference

This document is the reference for the public DXBridge v1 ABI exported by `dxbridge.dll`.

This page documents release `1.3.0`, including the additive capability-growth surface that ships without breaking the existing `1.x` contract.

- Public headers: `include/dxbridge/dxbridge.h`, `include/dxbridge/dxbridge_version.h`
- Current version: `1.5.0` <!-- x-release-please-version -->
- ABI style: C ABI, fixed-width integer types, opaque 64-bit handles
- Platforms: Windows 10/11, minimum OS noted in the public header as Windows 10 1809
- Minimum Windows SDK noted in the public header: `10.0.22000.0`

Compatibility note:

- `1.3.0` is the synchronized release marker in `version.txt`, `README.md`, and `include/dxbridge/dxbridge_version.h`.
- The additive `v1.3.0` delta is limited to the capability-discovery family: `DXBridge_QueryCapability()`, `DXBCapabilityQuery`, `DXBCapabilityInfo`, and the `DXBCapability` IDs.
- Existing exports `1` through `43`, handle widths, struct layouts, result codes, and current runtime semantics remain the protected `1.x` baseline.
- The release framing for that compatible minor lives in `docs/releases/v1.3.0-scope.md`, `docs/releases/v1.3.0-compatible-design.md`, and `docs/releases/v1.3.0-release-notes.md`.

## Read this first

This page is a reference document, not a step-by-step tutorial. If you are new to the repository, read these in order:

1. `README.md` for project scope and repository navigation
2. `docs/build-and-test.md` for setup and validation
3. `docs/examples.md` for runtime-specific sample paths
4. this page when implementing a binding or verifying ABI behavior

## Quick navigation

| Section | What it covers |
| --- | --- |
| `ABI Conventions` | versioning, handles, layout, error and logging rules |
| `Result Codes` | public status values returned by most functions |
| `Public Constants and Enum-Like Types` | backend, format, flag, and topology constants |
| `Struct Reference` | public input and output data structures |
| `Common Usage Patterns` | startup, enumeration, frame, resize, and error flows |
| `Function Reference` | per-function signatures, arguments, and notes |
| `Compatibility Notes for Binding Authors` | practical rules for non-C/C++ consumers |

## Consumer checklist

Before blaming the binding layer, confirm these basics:

- all handles are represented as unsigned 64-bit values
- every public input struct has `struct_version` initialized to `DXBRIDGE_STRUCT_VERSION`
- callbacks are kept alive for as long as native code may call them
- `DXBridge_GetLastError()` is read immediately after the failing call on the same thread
- windowed DX11 examples use a real Win32 `HWND`

## ABI Conventions

### Headers and linkage

- Include `include/dxbridge/dxbridge.h` for all public declarations.
- Include `include/dxbridge/dxbridge_version.h` for version macros.
- Functions are exported as `extern "C"` and use the default C calling convention for the compiler/toolchain.
- All public scalar ABI types use fixed-width integers from `<stdint.h>`.

### Version macros

| Macro | Meaning |
| --- | --- |
| `DXBRIDGE_VERSION_MAJOR` | Major version (`1`) |
| `DXBRIDGE_VERSION_MINOR` | Minor version (`3`) |
| `DXBRIDGE_VERSION_PATCH` | Patch version (`0`) |
| `DXBRIDGE_VERSION` | Packed version: `(major << 16) | (minor << 8) | patch` |
| `DXBRIDGE_STRUCT_VERSION` | Required struct version for all ABI structs (`1`) |

### Handles

All resource handles are opaque `uint64_t` values:

- `DXBDevice`
- `DXBSwapChain`
- `DXBBuffer`
- `DXBShader`
- `DXBPipeline`
- `DXBInputLayout`
- `DXBRTV`
- `DXBDSV`
- `DXBRenderTarget`

Use `DXBRIDGE_NULL_HANDLE` (`0`) as the null / invalid handle value.

DXBridge internally encodes handle type and generation to detect stale handles after destruction, but callers must treat handles as opaque tokens and never inspect their bits.

### Struct layout contract

Every public struct:

- is POD / plain C-compatible data
- uses only fixed-width fields and pointers
- places `struct_version` at offset `0`
- should be initialized with `DXBRIDGE_STRUCT_VERSION`

Recommended pattern:

```c
DXBDeviceDesc desc = {0};
desc.struct_version = DXBRIDGE_STRUCT_VERSION;
```

### Error model

- Most functions return `DXBResult`.
- `DXB_OK` is success.
- Any negative value is failure.
- Detailed text is stored in thread-local storage and retrieved with `DXBridge_GetLastError()`.
- The last native `HRESULT` for the current thread is retrieved with `DXBridge_GetLastHRESULT()`.
- Error state is thread-local: read it immediately after the failing call, on the same thread.

Important behavior:

- Validation failures generated entirely by DXBridge often leave `DXBridge_GetLastHRESULT()` as `0`.
- `DXBridge_GetLastError()` truncates safely to the caller buffer and always null-terminates when `buf_size > 0`.
- `DXBridge_GetLastError()` and `DXBridge_GetLastHRESULT()` are available even before `DXBridge_Init()`.

### Logging model

`DXBridge_SetLogCallback()` installs a process-wide callback.

Observed log levels used by v1:

| Level | Meaning |
| --- | --- |
| `0` | informational |
| `1` | warning |
| `2` | error |

Callback guidance:

- Keep the callback function alive for as long as native code may call it.
- Replacing the callback affects the whole process, not one device.
- Passing `NULL` unregisters logging.

### Threading

- Error state is thread-local.
- Log callback registration is process-wide.
- Handles are safe to pass by value between components, but rendering/state usage should be externally synchronized.
- For portability across backends, treat each device and its child objects as single-thread-affine unless your application serializes access.

### Ownership and lifetime

Resource ownership is explicit for almost all creation APIs:

| Resource | Created by | Released by |
| --- | --- | --- |
| `DXBDevice` | `DXBridge_CreateDevice()` | `DXBridge_DestroyDevice()` |
| `DXBSwapChain` | `DXBridge_CreateSwapChain()` | `DXBridge_DestroySwapChain()` |
| `DXBBuffer` | `DXBridge_CreateBuffer()` | `DXBridge_DestroyBuffer()` |
| `DXBShader` | `DXBridge_CompileShader()` | `DXBridge_DestroyShader()` |
| `DXBPipeline` | `DXBridge_CreatePipelineState()` | `DXBridge_DestroyPipeline()` |
| `DXBInputLayout` | `DXBridge_CreateInputLayout()` | `DXBridge_DestroyInputLayout()` |
| `DXBRTV` | `DXBridge_CreateRenderTargetView()` | `DXBridge_DestroyRTV()` |
| `DXBDSV` | `DXBridge_CreateDepthStencilView()` | `DXBridge_DestroyDSV()` |
| `DXBRenderTarget` | `DXBridge_GetBackBuffer()` | no public destroy function |

`DXBRenderTarget` notes:

- `DXBridge_GetBackBuffer()` returns a swap-chain-owned render-target handle.
- The public ABI does not expose `DestroyRenderTarget`.
- Treat back-buffer render targets as ephemeral handles used to create an RTV.
- Reacquire them after swap-chain resize.

Destruction notes:

- Destroy functions return `void` and silently ignore invalid handles.
- Destroy functions are also no-ops if DXBridge is not initialized.
- Destroy child resources before destroying the parent device or swap chain.

### Backend model

`DXBridge_Init()` selects the active backend for the process:

- `DXB_BACKEND_AUTO`: try DX12 first, then fall back to DX11
- `DXB_BACKEND_DX11`: force DX11
- `DXB_BACKEND_DX12`: require DX12

`DXBridge_SupportsFeature()` queries the active backend, not the machine in the abstract. For example, `DXB_FEATURE_DX12` returns `0` if DXBridge is currently running on the DX11 backend.

### Two-call enumeration pattern

`DXBridge_EnumerateAdapters()` uses the usual count-then-fill pattern:

1. Call with `out_buf = NULL` to get the required count.
2. Allocate that many `DXBAdapterInfo` entries.
3. Initialize each `struct_version` to `DXBRIDGE_STRUCT_VERSION`.
4. Call again with the buffer.

If the supplied count is smaller than the available adapter count, DXBridge fills as many entries as fit and writes back the number actually written.

## Result Codes

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_OK` | `0` | Success |
| `DXB_E_INVALID_HANDLE` | `-1` | Invalid, stale, or wrong-type handle |
| `DXB_E_DEVICE_LOST` | `-2` | Device removed/reset/lost |
| `DXB_E_OUT_OF_MEMORY` | `-3` | Allocation or descriptor space exhausted |
| `DXB_E_INVALID_ARG` | `-4` | Null pointer, unsupported enum value, invalid size, etc. |
| `DXB_E_NOT_SUPPORTED` | `-5` | Unsupported backend or capability |
| `DXB_E_SHADER_COMPILE` | `-6` | HLSL compilation failed |
| `DXB_E_NOT_INITIALIZED` | `-7` | `DXBridge_Init()` has not been called |
| `DXB_E_INVALID_STATE` | `-8` | Call order / lifecycle violation |
| `DXB_E_INTERNAL` | `-999` | Internal or unmapped failure |

## Public Constants and Enum-Like Types

### `DXBBackend`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_BACKEND_AUTO` | `0` | Try DX12, then DX11 |
| `DXB_BACKEND_DX11` | `1` | Direct3D 11 backend |
| `DXB_BACKEND_DX12` | `2` | Direct3D 12 backend |

### `DXBFormat`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_FORMAT_UNKNOWN` | `0` | Invalid / unspecified format |
| `DXB_FORMAT_RGBA8_UNORM` | `1` | Standard 8-bit UNORM RGBA render target |
| `DXB_FORMAT_RGBA16_FLOAT` | `2` | 16-bit float RGBA |
| `DXB_FORMAT_D32_FLOAT` | `3` | 32-bit float depth |
| `DXB_FORMAT_D24_UNORM_S8` | `4` | 24-bit depth + 8-bit stencil |
| `DXB_FORMAT_R32_UINT` | `5` | 32-bit unsigned integer, used for index buffers |
| `DXB_FORMAT_RG32_FLOAT` | `6` | Two 32-bit float channels |
| `DXB_FORMAT_RGB32_FLOAT` | `7` | Three 32-bit float channels |
| `DXB_FORMAT_RGBA32_FLOAT` | `8` | Four 32-bit float channels |

### `DXBDeviceFlags`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_DEVICE_FLAG_DEBUG` | `0x0001` | Request debug-layer / debug-device behavior where available |
| `DXB_DEVICE_FLAG_GPU_VALIDATION` | `0x0002` | Declared in the v1 ABI baseline and not currently acted on by the implementation |

### `DXBBufferFlags`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_BUFFER_FLAG_VERTEX` | `0x0001` | Vertex buffer |
| `DXB_BUFFER_FLAG_INDEX` | `0x0002` | Index buffer |
| `DXB_BUFFER_FLAG_CONSTANT` | `0x0004` | Constant/uniform buffer |
| `DXB_BUFFER_FLAG_DYNAMIC` | `0x0008` | Intended for frequent CPU updates |

### `DXBShaderStage`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_SHADER_STAGE_VERTEX` | `0` | Vertex shader |
| `DXB_SHADER_STAGE_PIXEL` | `1` | Pixel shader |

### `DXBPrimTopology`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_PRIM_TRIANGLELIST` | `0` | Triangle list |
| `DXB_PRIM_TRIANGLESTRIP` | `1` | Triangle strip |
| `DXB_PRIM_LINELIST` | `2` | Line list |
| `DXB_PRIM_POINTLIST` | `3` | Point list |

### `DXBInputSemantic`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_SEMANTIC_POSITION` | `0` | Position |
| `DXB_SEMANTIC_NORMAL` | `1` | Normal |
| `DXB_SEMANTIC_TEXCOORD` | `2` | Texture coordinate |
| `DXB_SEMANTIC_COLOR` | `3` | Color |

### `DXBFeatureFlag`

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_FEATURE_DX12` | `0x0001` | Active backend exposes DX12-specific capability |

`DXBridge_SupportsFeature()` remains the legacy active-backend check. It does not report machine-wide availability.

### `DXBCapability`

These IDs are used with the additive `DXBridge_QueryCapability()` family.

| Name | Value | Meaning |
| --- | ---: | --- |
| `DXB_CAPABILITY_BACKEND_AVAILABLE` | `1` | Requested backend can create at least one compatible device on this machine |
| `DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE` | `2` | Requested backend can use its debug layer on this machine |
| `DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE` | `3` | Requested backend can use GPU validation on this machine |
| `DXB_CAPABILITY_ADAPTER_COUNT` | `4` | Number of adapters exposed for the requested backend |
| `DXB_CAPABILITY_ADAPTER_SOFTWARE` | `5` | Whether a specific backend adapter is software / WARP |
| `DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL` | `6` | Highest feature level available for a specific backend adapter |

### Shader compile flags

`DXBShaderDesc.compile_flags` currently recognizes:

| Bit | Meaning |
| --- | --- |
| `0x1` | Debug shader compile (`D3DCompile` debug + skip optimization) |

The public header comments refer to this bit as `DXB_COMPILE_DEBUG`, but v1 does not export a public macro with that name. FFI bindings should define the bit explicitly.

## Struct Reference

### `DXBAdapterInfo`

Describes one enumerated adapter.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set by DXBridge to `DXBRIDGE_STRUCT_VERSION` |
| `index` | `uint32_t` | Adapter index used in `DXBDeviceDesc.adapter_index` |
| `description[128]` | `char[128]` | UTF-8 adapter description, null-terminated when possible |
| `dedicated_video_mem` | `uint64_t` | Dedicated VRAM bytes |
| `dedicated_system_mem` | `uint64_t` | Dedicated system memory bytes |
| `shared_system_mem` | `uint64_t` | Shared system memory bytes |
| `is_software` | `uint32_t` | `1` for WARP/software adapter, otherwise `0` |

### `DXBDeviceDesc`

Input for device creation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `backend` | `DXBBackend` | Keep consistent with the backend you initialized |
| `adapter_index` | `uint32_t` | `0` means default/high-performance adapter |
| `flags` | `DXBDeviceFlags` | Debug and related flags |

### `DXBSwapChainDesc`

Input for swap-chain creation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `hwnd` | `void*` | Real Win32 `HWND`; must be a valid window handle |
| `width` | `uint32_t` | Initial width in pixels |
| `height` | `uint32_t` | Initial height in pixels |
| `format` | `DXBFormat` | Typically `DXB_FORMAT_RGBA8_UNORM` |
| `buffer_count` | `uint32_t` | `2` is the normal default |
| `flags` | `uint32_t` | Reserved in v1; set to `0` |

Notes:

- DX11 enforces a minimum of 2 buffers.
- DX12 clamps values greater than 3 down to 3 and may log a warning.

### `DXBBufferDesc`

Input for buffer creation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `flags` | `DXBBufferFlags` | Buffer usage flags |
| `byte_size` | `uint32_t` | Required; must be non-zero |
| `stride` | `uint32_t` | Vertex stride in bytes; `0` for index/constant buffers |

Notes:

- DX11 pads constant buffers to 16-byte alignment internally.
- DX12 v1 places created buffers on an upload heap and keeps them CPU-writable for `UploadData()`.

### `DXBShaderDesc`

Input for shader compilation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `stage` | `DXBShaderStage` | Vertex or pixel shader |
| `source_code` | `const char*` | HLSL source text, UTF-8 |
| `source_size` | `uint32_t` | Byte length; `0` means `strlen(source_code)` |
| `source_name` | `const char*` | Optional label/path for diagnostics |
| `entry_point` | `const char*` | Entry-point function name |
| `target` | `const char*` | HLSL profile, for example `vs_5_0` or `ps_5_0` |
| `compile_flags` | `uint32_t` | Bitfield; `0x1` requests debug compile |

### `DXBInputElementDesc`

One vertex-input element.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `semantic` | `DXBInputSemantic` | Semantic name selector |
| `semantic_index` | `uint32_t` | For example `TEXCOORD0` -> `0` |
| `format` | `DXBFormat` | Element format |
| `input_slot` | `uint32_t` | Vertex buffer slot |
| `byte_offset` | `uint32_t` | Byte offset, or `0xFFFFFFFF` for append-aligned |

### `DXBDepthStencilDesc`

Input for depth/stencil creation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `format` | `DXBFormat` | `DXB_FORMAT_D32_FLOAT` or `DXB_FORMAT_D24_UNORM_S8` |
| `width` | `uint32_t` | Texture width |
| `height` | `uint32_t` | Texture height |

### `DXBPipelineDesc`

Input for pipeline creation.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `vs` | `DXBShader` | Vertex shader handle |
| `ps` | `DXBShader` | Pixel shader handle |
| `input_layout` | `DXBInputLayout` | Input layout handle, or null when no layout is needed |
| `topology` | `DXBPrimTopology` | Primitive topology |
| `depth_test` | `uint32_t` | Non-zero enables depth testing |
| `depth_write` | `uint32_t` | Non-zero enables depth writes |
| `cull_back` | `uint32_t` | Non-zero enables back-face culling |
| `wireframe` | `uint32_t` | Non-zero requests wireframe fill |

Compatibility note:

- On DX12, create the swap chain before creating pipeline state so the backend knows the active render-target format.

### `DXBViewport`

Viewport rectangle and depth range.

| Field | Type | Notes |
| --- | --- | --- |
| `x` | `float` | Left coordinate |
| `y` | `float` | Top coordinate |
| `width` | `float` | Width |
| `height` | `float` | Height |
| `min_depth` | `float` | Usually `0.0f` |
| `max_depth` | `float` | Usually `1.0f` |

### `DXBCapabilityQuery`

Input for `DXBridge_QueryCapability()`.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Set to `DXBRIDGE_STRUCT_VERSION` |
| `capability` | `DXBCapability` | Capability selector |
| `backend` | `DXBBackend` | Explicit backend target; current queries require `DX11` or `DX12` |
| `adapter_index` | `uint32_t` | Used by adapter-scoped capabilities |
| `format` | `DXBFormat` | Reserved for future additive queries |
| `device` | `DXBDevice` | Reserved for future additive queries |
| `reserved[4]` | `uint32_t[4]` | Set to zero |

### `DXBCapabilityInfo`

Output for `DXBridge_QueryCapability()`.

| Field | Type | Notes |
| --- | --- | --- |
| `struct_version` | `uint32_t` | Written by DXBridge as `DXBRIDGE_STRUCT_VERSION` |
| `capability` | `DXBCapability` | Echo of the requested capability |
| `backend` | `DXBBackend` | Echo of the requested backend |
| `adapter_index` | `uint32_t` | Echo of the requested adapter index |
| `supported` | `uint32_t` | `1` when the requested target resolves successfully |
| `value_u32` | `uint32_t` | Capability-specific numeric payload |
| `value_u64` | `uint64_t` | Reserved for future larger payloads |
| `reserved[4]` | `uint32_t[4]` | Reserved; currently zeroed |

Capability payload rules in `value_u32`:

- `DXB_CAPABILITY_BACKEND_AVAILABLE`: best available feature level for the backend when `supported != 0`
- `DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE`: best feature level reachable with the debug layer when `supported != 0`
- `DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE`: best feature level reachable with GPU validation when `supported != 0`
- `DXB_CAPABILITY_ADAPTER_COUNT`: adapter count; `supported` is always `1` if enumeration succeeded
- `DXB_CAPABILITY_ADAPTER_SOFTWARE`: `1` for software/WARP, `0` for hardware
- `DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL`: highest feature level for the selected adapter

## Common Usage Patterns

### Basic startup

```c
DXBridge_Init(DXB_BACKEND_AUTO);

DXBDeviceDesc dev = {0};
dev.struct_version = DXBRIDGE_STRUCT_VERSION;
dev.backend = DXB_BACKEND_AUTO;
dev.adapter_index = 0;

DXBDevice device = DXBRIDGE_NULL_HANDLE;
DXBridge_CreateDevice(&dev, &device);
```

### Capability preflight before init

```c
DXBCapabilityQuery query = {0};
query.struct_version = DXBRIDGE_STRUCT_VERSION;
query.capability = DXB_CAPABILITY_BACKEND_AVAILABLE;
query.backend = DXB_BACKEND_DX12;

DXBCapabilityInfo info = {0};
info.struct_version = DXBRIDGE_STRUCT_VERSION;

DXBResult r = DXBridge_QueryCapability(&query, &info);
if (r == DXB_OK && info.supported) {
    // info.value_u32 contains the best available feature level
}
```

Recommended interpretation:

1. use `DXBridge_QueryCapability()` before `DXBridge_Init()` when choosing backend/debug behavior
2. use `DXBridge_Init()` once you know the preferred backend
3. keep using `DXBridge_SupportsFeature()` only for active-backend checks after init

### Adapter enumeration

```c
uint32_t count = 0;
DXBridge_EnumerateAdapters(NULL, &count);

DXBAdapterInfo* adapters = calloc(count, sizeof(*adapters));
for (uint32_t i = 0; i < count; ++i) {
    adapters[i].struct_version = DXBRIDGE_STRUCT_VERSION;
}

DXBridge_EnumerateAdapters(adapters, &count);
```

### Typical frame flow

Portable frame flow:

1. `DXBridge_BeginFrame(device)`
2. `DXBridge_SetRenderTargets(device, rtv, dsv)`
3. clear / bind pipeline / bind buffers
4. `DXBridge_Draw()` or `DXBridge_DrawIndexed()`
5. `DXBridge_EndFrame(device)`
6. `DXBridge_Present(swapchain, sync_interval)`

Backend differences:

- DX11: `BeginFrame()` and `EndFrame()` are effectively validation no-ops.
- DX12: `BeginFrame()` / `EndFrame()` are mandatory lifecycle boundaries.
- DX12 rejects `Present()` and `ResizeSwapChain()` while a frame is still in progress.

### Resize flow

Recommended portable resize flow:

1. Stop rendering.
2. Destroy any RTVs/DSVs that reference the old swap-chain buffers.
3. Call `DXBridge_ResizeSwapChain()`.
4. Call `DXBridge_GetBackBuffer()` again.
5. Recreate RTV/DSV objects.

### Error retrieval

```c
DXBResult r = DXBridge_CreateSwapChain(device, &sc_desc, &swapchain);
if (r != DXB_OK) {
    char text[512] = {0};
    DXBridge_GetLastError(text, (int)sizeof(text));
    uint32_t hr = DXBridge_GetLastHRESULT();
}
```

## Function Reference

### Logging

#### `DXBridge_SetLogCallback`

```c
void DXBridge_SetLogCallback(DXBLogCallback cb, void* user_data);
```

- Registers or clears the global log callback.
- `cb`: callback pointer, or `NULL` to unregister.
- `user_data`: opaque pointer returned to each callback invocation.
- Return value: none.
- Notes: callback lifetime is owned by the caller.

## Exported Symbols

DXBridge v1 exports the following public entry points from `dxbridge.dll`.

Release framing note:

- ordinals `1` through `43` are the stable pre-`v1.3.0` baseline
- ordinal `44` is the additive capability-query entry point added by the compatible-growth minor

| Ordinal | Symbol |
| ---: | --- |
| 1 | `DXBridge_Init` |
| 2 | `DXBridge_Shutdown` |
| 3 | `DXBridge_GetVersion` |
| 4 | `DXBridge_SupportsFeature` |
| 5 | `DXBridge_GetLastError` |
| 6 | `DXBridge_GetLastHRESULT` |
| 7 | `DXBridge_SetLogCallback` |
| 8 | `DXBridge_EnumerateAdapters` |
| 9 | `DXBridge_CreateDevice` |
| 10 | `DXBridge_DestroyDevice` |
| 11 | `DXBridge_GetFeatureLevel` |
| 12 | `DXBridge_CreateSwapChain` |
| 13 | `DXBridge_DestroySwapChain` |
| 14 | `DXBridge_ResizeSwapChain` |
| 15 | `DXBridge_GetBackBuffer` |
| 16 | `DXBridge_CreateRenderTargetView` |
| 17 | `DXBridge_DestroyRTV` |
| 18 | `DXBridge_CreateDepthStencilView` |
| 19 | `DXBridge_DestroyDSV` |
| 20 | `DXBridge_CompileShader` |
| 21 | `DXBridge_DestroyShader` |
| 22 | `DXBridge_CreateInputLayout` |
| 23 | `DXBridge_DestroyInputLayout` |
| 24 | `DXBridge_CreatePipelineState` |
| 25 | `DXBridge_DestroyPipeline` |
| 26 | `DXBridge_CreateBuffer` |
| 27 | `DXBridge_UploadData` |
| 28 | `DXBridge_DestroyBuffer` |
| 29 | `DXBridge_BeginFrame` |
| 30 | `DXBridge_SetRenderTargets` |
| 31 | `DXBridge_ClearRenderTarget` |
| 32 | `DXBridge_ClearDepthStencil` |
| 33 | `DXBridge_SetViewport` |
| 34 | `DXBridge_SetPipeline` |
| 35 | `DXBridge_SetVertexBuffer` |
| 36 | `DXBridge_SetIndexBuffer` |
| 37 | `DXBridge_SetConstantBuffer` |
| 38 | `DXBridge_Draw` |
| 39 | `DXBridge_DrawIndexed` |
| 40 | `DXBridge_EndFrame` |
| 41 | `DXBridge_Present` |
| 42 | `DXBridge_GetNativeDevice` |
| 43 | `DXBridge_GetNativeContext` |
| 44 | `DXBridge_QueryCapability` |

### Initialization and process lifetime

#### `DXBridge_Init`

```c
DXBResult DXBridge_Init(DXBBackend backend_hint);
```

- Initializes DXBridge and selects the active backend.
- `backend_hint`: `AUTO`, `DX11`, or `DX12`.
- Returns `DXB_OK` on success, `DXB_E_NOT_SUPPORTED` if DX12 was explicitly requested but unavailable, or another mapped initialization failure.
- Notes: idempotent; calling it again after success returns `DXB_OK` and keeps the existing backend.

#### `DXBridge_Shutdown`

```c
void DXBridge_Shutdown(void);
```

- Shuts down the active backend and clears global state.
- Return value: none.
- Notes: safe to call when not initialized.

#### `DXBridge_GetVersion`

```c
uint32_t DXBridge_GetVersion(void);
```

- Returns the packed `DXBRIDGE_VERSION` integer.
- Return value: `(major << 16) | (minor << 8) | patch`.

#### `DXBridge_SupportsFeature`

```c
uint32_t DXBridge_SupportsFeature(uint32_t feature_flag);
```

- Queries support flags on the active backend.
- `feature_flag`: currently `DXB_FEATURE_DX12`.
- Return value: non-zero if supported, `0` otherwise.
- Notes: returns `0` before `DXBridge_Init()`.

#### `DXBridge_QueryCapability`

```c
DXBResult DXBridge_QueryCapability(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info);
```

- Queries additive compatibility and capability data without changing the active backend.
- `query`: capability request; set `struct_version` and choose an explicit backend.
- `out_info`: result payload; DXBridge rewrites `struct_version` and fills capability-specific fields.
- Returns `DXB_OK` on a valid query, or `DXB_E_INVALID_ARG` for null pointers, bad `struct_version`, invalid backend, or unknown capability ID.
- Notes:
  - works before `DXBridge_Init()`
  - is the only documented additive public API family in `v1.3.0`
  - does not reinterpret `DXBridge_SupportsFeature()`
  - current queries are machine/backend preflight checks, not active-device state inspection
  - out-of-range adapter queries return `DXB_OK` with `supported = 0`

### Error inspection

#### `DXBridge_GetLastError`

```c
void DXBridge_GetLastError(char* buf, int buf_size);
```

- Copies the calling thread's last error string into `buf`.
- `buf`: destination buffer.
- `buf_size`: destination size in bytes.
- Return value: none.
- Notes: no-op if `buf == NULL` or `buf_size <= 0`.

#### `DXBridge_GetLastHRESULT`

```c
uint32_t DXBridge_GetLastHRESULT(void);
```

- Returns the calling thread's last native `HRESULT`.
- Return value: last `HRESULT` as an unsigned 32-bit value, or `0` when none is set.

### Adapter enumeration

#### `DXBridge_EnumerateAdapters`

```c
DXBResult DXBridge_EnumerateAdapters(DXBAdapterInfo* out_buf, uint32_t* in_out_count);
```

- Enumerates adapters with a two-call pattern.
- `out_buf`: optional output array, or `NULL` on the first call.
- `in_out_count`: on input, capacity; on output, required count or written count.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, or `DXB_E_NOT_INITIALIZED`.
- Notes: initialize each output element's `struct_version` before the fill call.

### Device management

#### `DXBridge_CreateDevice`

```c
DXBResult DXBridge_CreateDevice(const DXBDeviceDesc* desc, DXBDevice* out_device);
```

- Creates a logical device on the active backend.
- `desc`: device creation descriptor.
- `out_device`: receives the new device handle.
- Returns `DXB_OK` or a failure such as `DXB_E_INVALID_ARG`, `DXB_E_NOT_INITIALIZED`, `DXB_E_NOT_SUPPORTED`, or `DXB_E_OUT_OF_MEMORY`.
- Notes: if `DXB_DEVICE_FLAG_DEBUG` is requested but the debug layer is unavailable, DX12 logs a warning and continues without it.

#### `DXBridge_DestroyDevice`

```c
void DXBridge_DestroyDevice(DXBDevice device);
```

- Destroys a device and releases backend resources.
- Return value: none.
- Notes: silently ignores invalid handles.

#### `DXBridge_GetFeatureLevel`

```c
uint32_t DXBridge_GetFeatureLevel(DXBDevice device);
```

- Returns the native D3D feature level for a device.
- Return value: raw feature-level code, for example `0xB000` (11.0), `0xB100` (11.1), `0xC000` (12.0), `0xC100` (12.1); returns `0` for an invalid device.

### Swap chains

#### `DXBridge_CreateSwapChain`

```c
DXBResult DXBridge_CreateSwapChain(DXBDevice device, const DXBSwapChainDesc* desc, DXBSwapChain* out_sc);
```

- Creates a windowed swap chain for a real Win32 window.
- `device`: owning device.
- `desc`: swap-chain descriptor.
- `out_sc`: receives the swap-chain handle.
- Returns `DXB_OK` or a validation/device/runtime error.
- Notes: `desc->hwnd` must be a valid `HWND`; DXBridge disables the Alt+Enter fullscreen shortcut on the created swap chain.

#### `DXBridge_DestroySwapChain`

```c
void DXBridge_DestroySwapChain(DXBSwapChain sc);
```

- Destroys a swap chain.
- Return value: none.

#### `DXBridge_ResizeSwapChain`

```c
DXBResult DXBridge_ResizeSwapChain(DXBSwapChain sc, uint32_t width, uint32_t height);
```

- Resizes swap-chain buffers.
- `sc`: swap-chain handle.
- `width`, `height`: new dimensions.
- Returns `DXB_OK` or a failure such as `DXB_E_INVALID_HANDLE`, `DXB_E_INVALID_STATE`, or `DXB_E_DEVICE_LOST`.
- Notes: destroy RTV/DSV objects that reference the old buffers first; DX12 rejects resize while a frame is active.

#### `DXBridge_GetBackBuffer`

```c
DXBResult DXBridge_GetBackBuffer(DXBSwapChain sc, DXBRenderTarget* out_rt);
```

- Retrieves a swap-chain-owned render-target handle representing the current back buffer.
- `sc`: swap-chain handle.
- `out_rt`: receives the render-target handle.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: there is no public destroy function for `DXBRenderTarget`; use the handle as input to `DXBridge_CreateRenderTargetView()`.

### Render-target and depth/stencil views

#### `DXBridge_CreateRenderTargetView`

```c
DXBResult DXBridge_CreateRenderTargetView(DXBDevice device, DXBRenderTarget rt, DXBRTV* out_rtv);
```

- Creates an RTV from a render target.
- `device`: owning device.
- `rt`: render-target handle, typically from `DXBridge_GetBackBuffer()`.
- `out_rtv`: receives the RTV handle.
- Returns `DXB_OK` or a validation/device/runtime error.

#### `DXBridge_DestroyRTV`

```c
void DXBridge_DestroyRTV(DXBRTV rtv);
```

- Destroys an RTV handle.
- Return value: none.

#### `DXBridge_CreateDepthStencilView`

```c
DXBResult DXBridge_CreateDepthStencilView(DXBDevice device, const DXBDepthStencilDesc* desc, DXBDSV* out_dsv);
```

- Creates a depth/stencil texture and its view.
- `device`: owning device.
- `desc`: depth-stencil descriptor.
- `out_dsv`: receives the DSV handle.
- Returns `DXB_OK` or a failure such as `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, `DXB_E_OUT_OF_MEMORY`, or `DXB_E_DEVICE_LOST`.

#### `DXBridge_DestroyDSV`

```c
void DXBridge_DestroyDSV(DXBDSV dsv);
```

- Destroys a DSV handle.
- Return value: none.

### Shader compilation

#### `DXBridge_CompileShader`

```c
DXBResult DXBridge_CompileShader(DXBDevice device, const DXBShaderDesc* desc, DXBShader* out_shader);
```

- Compiles HLSL source into a shader object.
- `device`: owning device.
- `desc`: shader descriptor.
- `out_shader`: receives the shader handle.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, `DXB_E_SHADER_COMPILE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: compile diagnostics are reported through `DXBridge_GetLastError()`; warnings may also be emitted through the log callback.

#### `DXBridge_DestroyShader`

```c
void DXBridge_DestroyShader(DXBShader shader);
```

- Destroys a shader handle.
- Return value: none.

### Input layouts

#### `DXBridge_CreateInputLayout`

```c
DXBResult DXBridge_CreateInputLayout(DXBDevice device, const DXBInputElementDesc* elements, uint32_t element_count, DXBShader vs, DXBInputLayout* out_layout);
```

- Creates an input-layout object.
- `device`: owning device.
- `elements`: element array.
- `element_count`: number of elements.
- `vs`: vertex shader used for validation/bytecode; required by DX11 and accepted by DX12.
- `out_layout`: receives the input-layout handle.
- Returns `DXB_OK` or a validation/device/runtime error.
- Notes: unknown element formats are rejected with `DXB_E_INVALID_ARG`; `byte_offset == 0xFFFFFFFF` means append-aligned.

#### `DXBridge_DestroyInputLayout`

```c
void DXBridge_DestroyInputLayout(DXBInputLayout layout);
```

- Destroys an input-layout handle.
- Return value: none.

### Pipeline state

#### `DXBridge_CreatePipelineState`

```c
DXBResult DXBridge_CreatePipelineState(DXBDevice device, const DXBPipelineDesc* desc, DXBPipeline* out_pipeline);
```

- Creates a graphics pipeline object.
- `device`: owning device.
- `desc`: pipeline descriptor.
- `out_pipeline`: receives the pipeline handle.
- Returns `DXB_OK` or a validation/device/runtime error.
- Notes: requires valid vertex/pixel shaders; input layout is optional. On DX12, creating the swap chain first is recommended so a render-target format is known.

#### `DXBridge_DestroyPipeline`

```c
void DXBridge_DestroyPipeline(DXBPipeline pipeline);
```

- Destroys a pipeline handle.
- Return value: none.

### Buffers

#### `DXBridge_CreateBuffer`

```c
DXBResult DXBridge_CreateBuffer(DXBDevice device, const DXBBufferDesc* desc, DXBBuffer* out_buf);
```

- Creates a GPU/interop buffer.
- `device`: owning device.
- `desc`: buffer descriptor.
- `out_buf`: receives the buffer handle.
- Returns `DXB_OK` or a validation/device/runtime error.
- Notes: `byte_size` must be non-zero.

#### `DXBridge_UploadData`

```c
DXBResult DXBridge_UploadData(DXBBuffer buf, const void* data, uint32_t byte_size);
```

- Uploads data into a buffer.
- `buf`: destination buffer.
- `data`: source bytes.
- `byte_size`: number of bytes to copy.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, `DXB_E_INVALID_STATE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: `byte_size` must not exceed the buffer capacity.

#### `DXBridge_DestroyBuffer`

```c
void DXBridge_DestroyBuffer(DXBBuffer buf);
```

- Destroys a buffer handle.
- Return value: none.

### Rendering and presentation

#### `DXBridge_BeginFrame`

```c
DXBResult DXBridge_BeginFrame(DXBDevice device);
```

- Starts a frame.
- `device`: owning device.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, `DXB_E_INVALID_STATE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: required framing call on DX12; DX11 treats it as a validated no-op.

#### `DXBridge_SetRenderTargets`

```c
DXBResult DXBridge_SetRenderTargets(DXBDevice device, DXBRTV rtv, DXBDSV dsv);
```

- Binds the current render target and optional depth/stencil view.
- `rtv`: render-target view handle.
- `dsv`: depth-stencil view handle, or `DXBRIDGE_NULL_HANDLE`.
- Returns `DXB_OK` or a validation/device/runtime error.

#### `DXBridge_ClearRenderTarget`

```c
DXBResult DXBridge_ClearRenderTarget(DXBRTV rtv, const float rgba[4]);
```

- Clears an RTV with RGBA float values.
- `rtv`: target RTV.
- `rgba`: pointer to four floats.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_ClearDepthStencil`

```c
DXBResult DXBridge_ClearDepthStencil(DXBDSV dsv, float depth, uint8_t stencil);
```

- Clears a DSV.
- `dsv`: target DSV.
- `depth`: depth clear value.
- `stencil`: stencil clear value.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_SetViewport`

```c
DXBResult DXBridge_SetViewport(DXBDevice device, const DXBViewport* vp);
```

- Sets the active viewport.
- `device`: owning device.
- `vp`: viewport data.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_SetPipeline`

```c
DXBResult DXBridge_SetPipeline(DXBDevice device, DXBPipeline pipeline);
```

- Binds the active pipeline state.
- `device`: owning device.
- `pipeline`: pipeline handle.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_SetVertexBuffer`

```c
DXBResult DXBridge_SetVertexBuffer(DXBDevice device, DXBBuffer buf, uint32_t stride, uint32_t offset);
```

- Binds a vertex buffer.
- `stride`: vertex stride in bytes.
- `offset`: byte offset.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_SetIndexBuffer`

```c
DXBResult DXBridge_SetIndexBuffer(DXBDevice device, DXBBuffer buf, DXBFormat fmt, uint32_t offset);
```

- Binds an index buffer.
- `fmt`: index format; use `DXB_FORMAT_R32_UINT` in v1.
- `offset`: byte offset.
- Returns `DXB_OK`, `DXB_E_INVALID_ARG`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_SetConstantBuffer`

```c
DXBResult DXBridge_SetConstantBuffer(DXBDevice device, uint32_t slot, DXBBuffer buf);
```

- Binds a constant buffer.
- `slot`: constant-buffer slot index.
- `buf`: buffer handle.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: DX11 binds the buffer to both VS and PS stages. DX12 v1 validates the device handle but does not currently perform an actual root-parameter binding and does not validate the buffer handle, so this call is effectively a no-op there after device validation.

#### `DXBridge_Draw`

```c
DXBResult DXBridge_Draw(DXBDevice device, uint32_t vertex_count, uint32_t start_vertex);
```

- Issues a non-indexed draw.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_DrawIndexed`

```c
DXBResult DXBridge_DrawIndexed(DXBDevice device, uint32_t index_count, uint32_t start_index, int32_t base_vertex);
```

- Issues an indexed draw.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, or `DXB_E_DEVICE_LOST` on DX12.

#### `DXBridge_EndFrame`

```c
DXBResult DXBridge_EndFrame(DXBDevice device);
```

- Ends the current frame.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, `DXB_E_INVALID_STATE`, or `DXB_E_DEVICE_LOST` on DX12.
- Notes: DX12 requires a matching `DXBridge_BeginFrame()` first; DX11 treats this as a validated no-op.

#### `DXBridge_Present`

```c
DXBResult DXBridge_Present(DXBSwapChain sc, uint32_t sync_interval);
```

- Presents the swap chain.
- `sync_interval`: standard DXGI-style sync interval.
- Returns `DXB_OK`, `DXB_E_INVALID_HANDLE`, `DXB_E_INVALID_STATE`, `DXB_E_DEVICE_LOST`, or another mapped runtime error.
- Notes: DX12 rejects `Present()` while a frame is still in progress.

### Native escape hatches

#### `DXBridge_GetNativeDevice`

```c
void* DXBridge_GetNativeDevice(DXBDevice device);
```

- Returns the backend-native device pointer.
- DX11: returns `ID3D11Device*`.
- DX12: returns `ID3D12Device*`.
- Return value: native pointer, or `NULL` for invalid device / no backend.
- Ownership notes:
  - DX11: borrowed pointer; if you keep it, take your own COM reference.
  - DX12: returned pointer is already `AddRef`'d; the caller must `Release()` it.

#### `DXBridge_GetNativeContext`

```c
void* DXBridge_GetNativeContext(DXBDevice device);
```

- Returns the backend-native immediate context, if one exists.
- DX11: returns `ID3D11DeviceContext*`.
- DX12: returns `NULL` because there is no immediate device context concept.
- Return value: native pointer or `NULL`.
- Ownership notes:
  - DX11: borrowed pointer; if you keep it, take your own COM reference.

## Compatibility Notes for Binding Authors

- Use 64-bit unsigned integers for all handles.
- Keep structs tightly aligned to the native 64-bit Windows ABI.
- Treat `void* hwnd` as a pointer-sized Win32 `HWND`.
- Initialize every `struct_version` field explicitly.
- Treat `DXBridge_QueryCapability()` as the pre-init compatibility family and `DXBridge_SupportsFeature()` as the active-backend compatibility helper.
- Model `DXBridge_GetLastError()` as thread-local state, not global state.
- Keep callbacks strongly reachable from GC-managed languages.
- Prefer the documented two-call pattern for adapter enumeration.
- Do not invent destroy calls for `DXBRenderTarget`; the ABI does not expose one.

## See Also

- `README.md`
- `docs/architecture.md`
- `docs/examples.md`
- `include/dxbridge/dxbridge.h`
- `include/dxbridge/dxbridge_version.h`
