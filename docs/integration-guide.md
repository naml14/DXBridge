# Integration Guide

This guide is the quickest path for consumers adopting the additive `DXBridge_QueryCapability()` family in `v1.3.0` without changing the existing `1.x` rendering flow.

## What `v1.3.0` adds

The compatible-growth release adds one public area and keeps everything else intentionally stable:

- `DXBridge_QueryCapability()`
- `DXBCapabilityQuery`
- `DXBCapabilityInfo`
- capability IDs for backend, debug layer, GPU validation, and adapter-scoped checks
- aligned helper coverage and scenario 06 examples in Bun, Node.js, Python, Go, and Rust
- cross-runtime launch and validation helpers for capability preflight

## What stays the same

- `DXBridge_Init()` still performs process-wide backend selection
- `DXBridge_SupportsFeature()` still reports active-backend state after init
- the existing device, swap-chain, frame, and presentation flow is unchanged
- existing `1.x` consumers do not need to adopt the new capability family unless they want pre-init discovery

## Recommended integration flow

For new integrations, use this order:

1. load `dxbridge.dll`
2. call `DXBridge_QueryCapability()` before `DXBridge_Init()` to discover machine/backend support
3. choose backend and optional debug behavior
4. call `DXBridge_Init()`
5. keep using the existing `1.x` device, swap-chain, and rendering flow unchanged

That keeps the new capability family additive instead of turning it into a new mandatory bootstrapping model.

## When to use each API

Use `DXBridge_QueryCapability()` when you need pre-init answers about the machine or a target backend:

- is DX11 available?
- is DX12 available?
- does this machine expose the debug layer for the chosen backend?
- is GPU validation available?
- how many adapters are exposed for the backend?
- is a specific adapter software/WARP?
- what is the max feature level for a specific adapter?

Use `DXBridge_SupportsFeature()` only after `DXBridge_Init()` when you want the legacy active-backend answer.

Important compatibility rule:

- `DXBridge_QueryCapability()` is pre-init/backend-targeted discovery
- `DXBridge_SupportsFeature()` remains active-backend scoped and is not redefined by `v1.3.0`

## Minimal native pattern

```c
DXBCapabilityQuery query = {0};
query.struct_version = DXBRIDGE_STRUCT_VERSION;
query.capability = DXB_CAPABILITY_BACKEND_AVAILABLE;
query.backend = DXB_BACKEND_DX12;

DXBCapabilityInfo info = {0};
info.struct_version = DXBRIDGE_STRUCT_VERSION;

DXBResult result = DXBridge_QueryCapability(&query, &info);
if (result == DXB_OK && info.supported) {
    // info.value_u32 now holds the best available feature level
}
```

## Helper coverage in the release-ready docs

The example binding layers now expose capability helpers for the most relevant onboarding runtimes:

- `examples/bun/dxbridge.ts`
- `examples/nodejs/dxbridge.js`
- `examples/python/dxbridge_ctypes.py`
- `examples/go/internal/dxbridge/dxbridge.go`
- `examples/rust/src/dxbridge.rs`

Each helper layer now provides:

- a raw capability query wrapper
- convenience helpers for backend availability, debug layer, GPU validation, adapter count, software adapter detection, and adapter max feature level
- feature-level formatting helpers for readable output

## DLL discovery guidance

The example helpers still support explicit `--dll` overrides and `DXBRIDGE_DLL`, and the release-ready onboarding path keeps the common search paths aligned so capability discovery is usable immediately after load.

Recommended onboarding command families:

- Bun: `bun run examples/bun/example06_capability_preflight.ts`
- Node.js: `node examples/nodejs/example06_capability_preflight.js`
- Python: `python examples/python/example_06_capability_preflight.py`
- Go: `go run ./cmd/example06_capability_preflight`
- Rust: `cargo run --manifest-path examples\rust\Cargo.toml --bin example06_capability_preflight`
- Cross-runtime PowerShell entry point: `pwsh -File examples/run_capability_preflight.ps1`

## Common interpretation rules

- `supported = 1` means the query resolved for the requested backend or adapter target
- `supported = 0` with `DXB_OK` means the query shape is valid, but the requested target is unavailable or out of range
- backend-availability and validation queries report their numeric payload in `value_u32` as a D3D feature level
- adapter count reports the number of backend-visible adapters in `value_u32`
- adapter software reports `1` for software/WARP and `0` for hardware

## Scenario 06 CLI contract

The refreshed capability-preflight examples now share the same shape where the runtime allows it:

- `--backend all|dx11|dx12` scopes the pre-init queries
- `--compare-active dx11|dx12` optionally initializes one backend afterward to contrast legacy `DXBridge_SupportsFeature()` behavior
- `--dll PATH` always remains the escape hatch for explicit DLL selection

## Compatibility reminder

`v1.3.0` does not introduce a new mandatory runtime model. Existing `1.x` consumers can ignore these helpers entirely and continue using the original flow.

## Release-facing references

Use these documents together when preparing or reviewing the `v1.3.0` publication story:

- `docs/releases/v1.3.0-scope.md`
- `docs/releases/v1.3.0-compatible-design.md`
- `docs/releases/v1.3.0-release-notes.md`
- `docs/releases/v1.3.0-publishing-checklist.md`

## Related docs

- `README.md`
- `docs/api-reference.md`
- `docs/examples.md`
- `examples/run_capability_preflight.ps1`
- `docs/releases/v1.3.0-scope.md`
- `docs/releases/v1.3.0-compatible-design.md`
