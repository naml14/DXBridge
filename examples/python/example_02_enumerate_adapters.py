from __future__ import annotations

import argparse

from dxbridge_ctypes import BACKEND_NAMES, DXBridgeLibrary, format_bytes


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Enumerate DXBridge adapters using the two-call ABI pattern."
    )
    parser.add_argument("--dll", help="Optional explicit path to dxbridge.dll")
    parser.add_argument(
        "--backend",
        choices=sorted(BACKEND_NAMES),
        default="auto",
        help="Backend hint used during DXBridge_Init before enumeration",
    )
    args = parser.parse_args()

    dxbridge = DXBridgeLibrary(args.dll)
    print(f"Loaded: {dxbridge.path}")

    try:
        dxbridge.init(BACKEND_NAMES[args.backend])
        adapters = dxbridge.enumerate_adapters()
        if not adapters:
            print("No adapters reported by DXBridge.")
            return 0

        print(f"Found {len(adapters)} adapter(s):")
        for adapter in adapters:
            description = adapter.description.decode("utf-8", errors="replace").rstrip(
                "\x00"
            )
            adapter_kind = "software" if adapter.is_software else "hardware"
            print(
                f"- #{adapter.index}: {description} [{adapter_kind}] | "
                f"VRAM={format_bytes(adapter.dedicated_video_mem)} | "
                f"Shared={format_bytes(adapter.shared_system_mem)}"
            )
    finally:
        dxbridge.shutdown()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
