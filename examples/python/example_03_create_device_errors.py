from __future__ import annotations

import argparse
import ctypes

from dxbridge_ctypes import (
    BACKEND_NAMES,
    DXBDevice,
    DXBridgeLibrary,
    format_hresult,
    make_versioned,
    DXBDeviceDesc,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a DXBridge device and show immediate error handling."
    )
    parser.add_argument("--dll", help="Optional explicit path to dxbridge.dll")
    parser.add_argument(
        "--backend",
        choices=sorted(BACKEND_NAMES),
        default="dx11",
        help="Backend used for the successful CreateDevice call",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Request DXB_DEVICE_FLAG_DEBUG on the successful CreateDevice call",
    )
    args = parser.parse_args()

    dxbridge = DXBridgeLibrary(args.dll)
    print(f"Loaded: {dxbridge.path}")

    print("1) Intentionally failing CreateDevice before DXBridge_Init...")
    not_initialized_device = DXBDevice(0)
    result = dxbridge.dll.DXBridge_CreateDevice(
        None, ctypes.byref(not_initialized_device)
    )
    print(f"   result={result} | {dxbridge.describe_failure(result)}")
    print(
        "   Read GetLastError immediately: the buffer is thread-local and can be overwritten by later calls."
    )

    device = 0
    try:
        print("2) Initializing DXBridge...")
        dxbridge.init(BACKEND_NAMES[args.backend])

        print("3) Creating a real device...")
        flags = 0x0001 if args.debug else 0
        desc = make_versioned(
            DXBDeviceDesc,
            backend=BACKEND_NAMES[args.backend],
            adapter_index=0,
            flags=flags,
        )
        handle = DXBDevice(0)
        dxbridge.raise_for_result(
            dxbridge.dll.DXBridge_CreateDevice(
                ctypes.byref(desc), ctypes.byref(handle)
            ),
            "DXBridge_CreateDevice",
        )
        device = int(handle.value)

        feature_level = dxbridge.get_feature_level(device)
        print(f"   device handle: {device}")
        print(f"   feature level: {format_hresult(feature_level)}")
        print("   Device creation succeeded.")
    finally:
        if device:
            dxbridge.destroy_device(device)
        dxbridge.shutdown()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
