from __future__ import annotations

import argparse

from dxbridge_ctypes import (
    BACKEND_NAMES,
    DXB_FEATURE_DX12,
    DXBridgeLibrary,
    LOG_LEVEL_NAMES,
    parse_version,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Load dxbridge.dll, print version, and receive logs."
    )
    parser.add_argument("--dll", help="Optional explicit path to dxbridge.dll")
    parser.add_argument(
        "--backend",
        choices=sorted(BACKEND_NAMES),
        default="auto",
        help="Backend hint passed to DXBridge_Init",
    )
    args = parser.parse_args()

    dxbridge = DXBridgeLibrary(args.dll)
    print(f"Loaded: {dxbridge.path}")

    def on_log(level: int, message: str, _user_data) -> None:
        name = LOG_LEVEL_NAMES.get(level, f"LEVEL_{level}")
        print(f"[log:{name}] {message}")

    dxbridge.set_log_callback(on_log)
    try:
        version = dxbridge.get_version()
        print(f"DXBridge version: {parse_version(version)} ({version:#010x})")

        dxbridge.init(BACKEND_NAMES[args.backend])
        supports_dx12 = bool(dxbridge.dll.DXBridge_SupportsFeature(DXB_FEATURE_DX12))
        print(f"DX12 feature flag reported by active backend: {supports_dx12}")
        print("Initialization and shutdown completed successfully.")
        dxbridge.shutdown()
    finally:
        dxbridge.set_log_callback(None)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
