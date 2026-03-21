from __future__ import annotations

import argparse
import ctypes
import math
import time
from ctypes import wintypes

from dxbridge_ctypes import (
    DXB_BACKEND_DX11,
    DXB_BUFFER_FLAG_DYNAMIC,
    DXB_BUFFER_FLAG_VERTEX,
    DXB_COMPILE_DEBUG,
    DXB_DEVICE_FLAG_DEBUG,
    DXB_FORMAT_RG32_FLOAT,
    DXB_FORMAT_RGB32_FLOAT,
    DXB_SEMANTIC_COLOR,
    DXB_SEMANTIC_POSITION,
    DXB_SHADER_STAGE_PIXEL,
    DXB_SHADER_STAGE_VERTEX,
    DXBInputElementDesc,
    DXBridgeLibrary,
    make_versioned,
)


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


VS_SOURCE = """
struct VSInput {
    float2 pos : POSITION;
    float3 col : COLOR;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

PSInput main(VSInput v) {
    PSInput o;
    o.pos = float4(v.pos, 0.0f, 1.0f);
    o.col = v.col;
    return o;
}
"""

PS_SOURCE = """
struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

float4 main(PSInput p) : SV_TARGET {
    return float4(p.col, 1.0f);
}
"""


class Win32Window:
    def __init__(self, title: str, width: int, height: int, visible: bool):
        self.title = title
        self.width = width
        self.height = height
        self.visible = visible
        self.hinstance = kernel32.GetModuleHandleW(None)
        self.class_name = f"DXBridgePythonMovingTriangle_{id(self)}"
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


def make_vertex_data(offset_x: float, offset_y: float):
    top = (0.0 + offset_x, 0.52 + offset_y, 1.0, 0.1, 0.1)
    left = (-0.42 + offset_x, -0.38 + offset_y, 0.1, 1.0, 0.1)
    right = (0.42 + offset_x, -0.38 + offset_y, 0.1, 0.3, 1.0)
    return (ctypes.c_float * 15)(*(top + left + right))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Renderiza un triangulo DX11 animado usando dxbridge.dll desde Python ctypes."
    )
    parser.add_argument("--dll", help="Ruta opcional explicita a dxbridge.dll")
    parser.add_argument("--width", type=int, default=960, help="Ancho de ventana")
    parser.add_argument("--height", type=int, default=540, help="Alto de ventana")
    parser.add_argument(
        "--frames",
        type=int,
        default=0,
        help="Cantidad de frames (0 = hasta cerrar la ventana)",
    )
    parser.add_argument(
        "--speed",
        type=float,
        default=1.6,
        help="Velocidad de oscilacion horizontal del triangulo",
    )
    parser.add_argument(
        "--amplitude",
        type=float,
        default=0.38,
        help="Amplitud horizontal del movimiento en NDC",
    )
    parser.add_argument(
        "--hidden",
        action="store_true",
        help="Crea la ventana sin mostrarla (util para smoke test)",
    )
    parser.add_argument(
        "--debug", action="store_true", help="Solicita DXB_DEVICE_FLAG_DEBUG"
    )
    parser.add_argument(
        "--sync-interval",
        type=int,
        default=1,
        help="Sync interval para present (1 = vsync, 0 = sin vsync)",
    )
    args = parser.parse_args()

    window = Win32Window(
        "DXBridge Python - Moving Triangle", args.width, args.height, not args.hidden
    )
    dxbridge = DXBridgeLibrary(args.dll)

    device = 0
    swapchain = 0
    backbuffer = 0
    rtv = 0
    vs = 0
    ps = 0
    input_layout = 0
    pipeline = 0
    vertex_buffer = 0

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

        vs = dxbridge.compile_shader(
            device,
            DXB_SHADER_STAGE_VERTEX,
            VS_SOURCE,
            "moving_triangle_vs",
            "main",
            "vs_5_0",
            compile_flags=DXB_COMPILE_DEBUG,
        )
        ps = dxbridge.compile_shader(
            device,
            DXB_SHADER_STAGE_PIXEL,
            PS_SOURCE,
            "moving_triangle_ps",
            "main",
            "ps_5_0",
            compile_flags=DXB_COMPILE_DEBUG,
        )

        elements = [
            make_versioned(
                DXBInputElementDesc,
                semantic=DXB_SEMANTIC_POSITION,
                semantic_index=0,
                format=DXB_FORMAT_RG32_FLOAT,
                input_slot=0,
                byte_offset=0,
            ),
            make_versioned(
                DXBInputElementDesc,
                semantic=DXB_SEMANTIC_COLOR,
                semantic_index=0,
                format=DXB_FORMAT_RGB32_FLOAT,
                input_slot=0,
                byte_offset=8,
            ),
        ]
        input_layout = dxbridge.create_input_layout(device, elements, vs)
        pipeline = dxbridge.create_pipeline_state(device, vs, ps, input_layout)

        vertex_buffer = dxbridge.create_buffer(
            device,
            flags=DXB_BUFFER_FLAG_VERTEX | DXB_BUFFER_FLAG_DYNAMIC,
            byte_size=15 * ctypes.sizeof(ctypes.c_float),
            stride=5 * ctypes.sizeof(ctypes.c_float),
        )

        start_time = time.perf_counter()
        frame_index = 0
        print("Rendering triangle animation. Cierra la ventana para salir.")

        while window.pump_messages():
            elapsed = time.perf_counter() - start_time
            offset_x = args.amplitude * math.sin(elapsed * args.speed)
            offset_y = 0.06 * math.sin(elapsed * (args.speed * 1.9))

            vertices = make_vertex_data(offset_x, offset_y)
            dxbridge.upload_data(vertex_buffer, vertices)

            dxbridge.begin_frame(device)
            dxbridge.set_render_targets(device, rtv)
            dxbridge.clear_render_target(rtv, (0.04, 0.05, 0.08, 1.0))
            dxbridge.set_viewport(
                device,
                x=0.0,
                y=0.0,
                width=float(args.width),
                height=float(args.height),
            )
            dxbridge.set_pipeline(device, pipeline)
            dxbridge.set_vertex_buffer(
                device,
                vertex_buffer,
                stride=5 * ctypes.sizeof(ctypes.c_float),
                offset=0,
            )
            dxbridge.draw(device, vertex_count=3, start_vertex=0)
            dxbridge.end_frame(device)
            dxbridge.present(swapchain, max(0, args.sync_interval))

            frame_index += 1
            if args.frames > 0 and frame_index >= args.frames:
                if window.hwnd:
                    user32.DestroyWindow(window.hwnd)
                break

        print(f"Done. Rendered frames: {frame_index}")
    finally:
        if vertex_buffer:
            dxbridge.destroy_buffer(vertex_buffer)
        if pipeline:
            dxbridge.destroy_pipeline(pipeline)
        if input_layout:
            dxbridge.destroy_input_layout(input_layout)
        if ps:
            dxbridge.destroy_shader(ps)
        if vs:
            dxbridge.destroy_shader(vs)
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
