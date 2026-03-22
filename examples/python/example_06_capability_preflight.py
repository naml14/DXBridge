from __future__ import annotations

import argparse

from dxbridge_ctypes import (
    BACKEND_NAMES,
    DXB_FEATURE_DX12,
    DXB_BACKEND_DX11,
    DXB_BACKEND_DX12,
    DXBridgeLibrary,
    format_feature_level,
)

BACKENDS = [
    (DXB_BACKEND_DX11, "DX11"),
    (DXB_BACKEND_DX12, "DX12"),
]


def select_backends(name: str) -> list[tuple[int, str]]:
    if name == "all":
        return BACKENDS

    backend = BACKEND_NAMES[name]
    for backend_id, label in BACKENDS:
        if backend_id == backend:
            return [(backend_id, label)]
    raise ValueError(f"Unsupported backend for capability preflight: {name}")


def parse_compare_backend(name: str | None) -> tuple[int, str] | None:
    if not name:
        return None
    backend = BACKEND_NAMES[name]
    if backend == 0:
        raise ValueError("--compare-active must be dx11 or dx12.")
    for backend_id, label in BACKENDS:
        if backend_id == backend:
            return (backend_id, label)
    raise ValueError(f"Unsupported compare backend: {name}")


def print_backend(dxbridge: DXBridgeLibrary, backend: int, label: str) -> None:
    available = dxbridge.query_backend_available(backend)
    debug = dxbridge.query_debug_layer_available(backend)
    gpu = dxbridge.query_gpu_validation_available(backend)
    count = dxbridge.query_adapter_count(backend)

    print(f"{label}:")
    print(
        f"  available: {bool(available.supported)} "
        f"(best FL {format_feature_level(int(available.value_u32))})"
    )
    print(
        f"  debug layer: {bool(debug.supported)} "
        f"(best FL {format_feature_level(int(debug.value_u32))})"
    )
    print(
        f"  GPU validation: {bool(gpu.supported)} "
        f"(best FL {format_feature_level(int(gpu.value_u32))})"
    )
    print(f"  adapter count: {int(count.value_u32)}")

    for adapter_index in range(int(count.value_u32)):
        software = dxbridge.query_adapter_software(backend, adapter_index)
        feature_level = dxbridge.query_adapter_max_feature_level(backend, adapter_index)
        print(
            f"  adapter {adapter_index}: software={bool(software.value_u32)} "
            f"maxFL={format_feature_level(int(feature_level.value_u32))}"
        )


def print_legacy_comparison(
    dxbridge: DXBridgeLibrary, backend: int, label: str
) -> None:
    print("Legacy comparison after DXBridge_Init():")
    dxbridge.init(backend)
    try:
        print(f"  active backend: {label}")
        print(
            "  DXBridge_SupportsFeature(DXB_FEATURE_DX12): "
            f"{dxbridge.supports_feature(DXB_FEATURE_DX12)}"
        )
        print(
            "  This remains an active-backend check, not a machine-wide preflight query."
        )
    finally:
        dxbridge.shutdown()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run pre-init DXBridge capability discovery for DX11 and DX12."
    )
    parser.add_argument("--dll", help="Optional explicit path to dxbridge.dll")
    parser.add_argument(
        "--backend",
        choices=["all", "dx11", "dx12"],
        default="all",
        help="Backend set to inspect during the pre-init pass",
    )
    parser.add_argument(
        "--compare-active",
        choices=["dx11", "dx12"],
        help="Optionally initialize one backend after preflight to contrast DXBridge_SupportsFeature()",
    )
    args = parser.parse_args()

    selected_backends = select_backends(args.backend)
    compare_backend = parse_compare_backend(args.compare_active)
    dxbridge = DXBridgeLibrary(args.dll)
    print(f"Loaded: {dxbridge.path}")
    print("Scenario 06 - Capability preflight")
    print("Purpose: inspect backend readiness before DXBridge_Init().")
    print("Backends: " + ", ".join(label for _, label in selected_backends))
    print("Pre-init capability discovery:")
    for backend, label in selected_backends:
        print_backend(dxbridge, backend, label)
    if compare_backend:
        print_legacy_comparison(dxbridge, compare_backend[0], compare_backend[1])
    else:
        print(
            "Tip: pass --compare-active dx11 or --compare-active dx12 to contrast "
            "the legacy active-backend check."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
