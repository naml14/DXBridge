from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes

from dxbridge_ctypes import DXB_BACKEND_DX11, DXBridgeLibrary, DXB_DEVICE_FLAG_DEBUG


user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

LRESULT = ctypes.c_ssize_t
WNDPROC = ctypes.WINFUNCTYPE(
    LRESULT, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM
)

CS_HREDRAW = 0x0002
CS_VREDRAW = 0x0001
WS_OVERLAPPEDWINDOW = 0x00CF0000
CW_USEDEFAULT = 0x80000000
SW_HIDE = 0
SW_SHOW = 5
PM_REMOVE = 0x0001
WM_DESTROY = 0x0002
WM_CLOSE = 0x0010
WM_QUIT = 0x0012


class WNDCLASSEXW(ctypes.Structure):
    _fields_ = [
        ("cbSize", ctypes.c_uint32),
        ("style", ctypes.c_uint32),
        ("lpfnWndProc", WNDPROC),
        ("cbClsExtra", ctypes.c_int),
        ("cbWndExtra", ctypes.c_int),
        ("hInstance", wintypes.HINSTANCE),
        ("hIcon", wintypes.HICON),
        ("hCursor", wintypes.HCURSOR),
        ("hbrBackground", wintypes.HBRUSH),
        ("lpszMenuName", wintypes.LPCWSTR),
        ("lpszClassName", wintypes.LPCWSTR),
        ("hIconSm", wintypes.HICON),
    ]


kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
kernel32.GetModuleHandleW.restype = wintypes.HMODULE

user32.LoadCursorW.restype = wintypes.HCURSOR
user32.RegisterClassExW.argtypes = [ctypes.POINTER(WNDCLASSEXW)]
user32.RegisterClassExW.restype = ctypes.c_uint16
user32.CreateWindowExW.argtypes = [
    wintypes.DWORD,
    wintypes.LPCWSTR,
    wintypes.LPCWSTR,
    wintypes.DWORD,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.HWND,
    wintypes.HMENU,
    wintypes.HINSTANCE,
    wintypes.LPVOID,
]
user32.CreateWindowExW.restype = wintypes.HWND
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.ShowWindow.restype = ctypes.c_bool
user32.UpdateWindow.argtypes = [wintypes.HWND]
user32.UpdateWindow.restype = ctypes.c_bool
user32.PeekMessageW.argtypes = [
    ctypes.POINTER(wintypes.MSG),
    wintypes.HWND,
    wintypes.UINT,
    wintypes.UINT,
    wintypes.UINT,
]
user32.PeekMessageW.restype = ctypes.c_bool
user32.TranslateMessage.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.TranslateMessage.restype = ctypes.c_bool
user32.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.DispatchMessageW.restype = LRESULT
user32.DefWindowProcW.argtypes = [
    wintypes.HWND,
    wintypes.UINT,
    wintypes.WPARAM,
    wintypes.LPARAM,
]
user32.DefWindowProcW.restype = LRESULT
user32.DestroyWindow.argtypes = [wintypes.HWND]
user32.DestroyWindow.restype = ctypes.c_bool
user32.PostQuitMessage.argtypes = [ctypes.c_int]
user32.PostQuitMessage.restype = None
user32.UnregisterClassW.argtypes = [wintypes.LPCWSTR, wintypes.HINSTANCE]
user32.UnregisterClassW.restype = ctypes.c_bool


class Win32Window:
    def __init__(self, title: str, width: int, height: int, visible: bool):
        self.title = title
        self.width = width
        self.height = height
        self.visible = visible
        self.hinstance = kernel32.GetModuleHandleW(None)
        self.class_name = f"DXBridgePythonClearWindow_{id(self)}"
        self.hwnd = None
        self._closed = False
        self._wndproc = WNDPROC(self._handle_message)

    def create(self) -> int:
        window_class = WNDCLASSEXW()
        window_class.cbSize = ctypes.sizeof(WNDCLASSEXW)
        window_class.style = CS_HREDRAW | CS_VREDRAW
        window_class.lpfnWndProc = self._wndproc
        window_class.hInstance = self.hinstance
        window_class.hCursor = user32.LoadCursorW(None, ctypes.c_void_p(32512))
        window_class.lpszClassName = self.class_name

        atom = user32.RegisterClassExW(ctypes.byref(window_class))
        if not atom:
            raise ctypes.WinError()

        hwnd = user32.CreateWindowExW(
            0,
            self.class_name,
            self.title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            self.width,
            self.height,
            None,
            None,
            self.hinstance,
            None,
        )
        if not hwnd:
            user32.UnregisterClassW(self.class_name, self.hinstance)
            raise ctypes.WinError()

        self.hwnd = hwnd
        user32.ShowWindow(hwnd, SW_SHOW if self.visible else SW_HIDE)
        user32.UpdateWindow(hwnd)
        return int(hwnd)

    def pump_messages(self) -> bool:
        msg = wintypes.MSG()
        while user32.PeekMessageW(ctypes.byref(msg), None, 0, 0, PM_REMOVE):
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))
            if msg.message == WM_QUIT:
                self._closed = True
        return not self._closed

    def destroy(self) -> None:
        if self.hwnd:
            user32.DestroyWindow(self.hwnd)
            self.hwnd = None
        user32.UnregisterClassW(self.class_name, self.hinstance)

    def _handle_message(self, hwnd, message, wparam, lparam):
        if message == WM_CLOSE:
            user32.DestroyWindow(hwnd)
            return 0
        if message == WM_DESTROY:
            self._closed = True
            if self.hwnd == hwnd:
                self.hwnd = None
            user32.PostQuitMessage(0)
            return 0
        return user32.DefWindowProcW(hwnd, message, wparam, lparam)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a real Win32 window and clear it through DXBridge DX11."
    )
    parser.add_argument("--dll", help="Optional explicit path to dxbridge.dll")
    parser.add_argument("--width", type=int, default=640, help="Window width in pixels")
    parser.add_argument(
        "--height", type=int, default=360, help="Window height in pixels"
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=180,
        help="How many frames to render before auto-closing",
    )
    parser.add_argument(
        "--color",
        type=float,
        nargs=4,
        metavar=("R", "G", "B", "A"),
        default=(0.08, 0.16, 0.32, 1.0),
        help="Clear color as four floats",
    )
    parser.add_argument(
        "--hidden",
        action="store_true",
        help="Create a real HWND without showing the window",
    )
    parser.add_argument(
        "--debug", action="store_true", help="Request DX11 debug device creation"
    )
    args = parser.parse_args()

    window = Win32Window(
        "DXBridge Python Clear Window", args.width, args.height, not args.hidden
    )
    dxbridge = DXBridgeLibrary(args.dll)

    device = 0
    swapchain = 0
    rtv = 0
    try:
        hwnd = window.create()
        print(f"Loaded: {dxbridge.path}")
        print(f"Created HWND: 0x{hwnd:016X}")

        dxbridge.init(DXB_BACKEND_DX11)
        device = dxbridge.create_device(
            DXB_BACKEND_DX11,
            adapter_index=0,
            flags=DXB_DEVICE_FLAG_DEBUG if args.debug else 0,
        )
        swapchain = dxbridge.create_swapchain(device, hwnd, args.width, args.height)

        backbuffer = dxbridge.get_back_buffer(swapchain)
        rtv = dxbridge.create_rtv(device, backbuffer)
        print("Rendering frames. Close the window manually or wait for auto-close.")

        for frame_index in range(args.frames):
            if not window.pump_messages():
                break
            dxbridge.begin_frame(device)
            dxbridge.set_render_targets(device, rtv)
            dxbridge.clear_render_target(rtv, args.color)
            dxbridge.end_frame(device)
            dxbridge.present(swapchain, 1)
            if frame_index + 1 == args.frames and window.hwnd:
                user32.DestroyWindow(window.hwnd)

        print("Done.")
    finally:
        if rtv:
            dxbridge.destroy_rtv(rtv)
        if swapchain:
            dxbridge.destroy_swapchain(swapchain)
        if device:
            dxbridge.destroy_device(device)
        dxbridge.shutdown()
        window.destroy()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
