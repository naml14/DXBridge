from __future__ import annotations

import ctypes
import os
from pathlib import Path


DXBRIDGE_NULL_HANDLE = 0
DXBRIDGE_STRUCT_VERSION = 1

DXB_OK = 0
DXB_E_INVALID_HANDLE = -1
DXB_E_DEVICE_LOST = -2
DXB_E_OUT_OF_MEMORY = -3
DXB_E_INVALID_ARG = -4
DXB_E_NOT_SUPPORTED = -5
DXB_E_SHADER_COMPILE = -6
DXB_E_NOT_INITIALIZED = -7
DXB_E_INVALID_STATE = -8
DXB_E_INTERNAL = -999

RESULT_NAMES = {
    DXB_OK: "DXB_OK",
    DXB_E_INVALID_HANDLE: "DXB_E_INVALID_HANDLE",
    DXB_E_DEVICE_LOST: "DXB_E_DEVICE_LOST",
    DXB_E_OUT_OF_MEMORY: "DXB_E_OUT_OF_MEMORY",
    DXB_E_INVALID_ARG: "DXB_E_INVALID_ARG",
    DXB_E_NOT_SUPPORTED: "DXB_E_NOT_SUPPORTED",
    DXB_E_SHADER_COMPILE: "DXB_E_SHADER_COMPILE",
    DXB_E_NOT_INITIALIZED: "DXB_E_NOT_INITIALIZED",
    DXB_E_INVALID_STATE: "DXB_E_INVALID_STATE",
    DXB_E_INTERNAL: "DXB_E_INTERNAL",
}

DXB_BACKEND_AUTO = 0
DXB_BACKEND_DX11 = 1
DXB_BACKEND_DX12 = 2

BACKEND_NAMES = {
    "auto": DXB_BACKEND_AUTO,
    "dx11": DXB_BACKEND_DX11,
    "dx12": DXB_BACKEND_DX12,
}

DXB_FORMAT_UNKNOWN = 0
DXB_FORMAT_RGBA8_UNORM = 1
DXB_FORMAT_D32_FLOAT = 3
DXB_FORMAT_RG32_FLOAT = 6
DXB_FORMAT_RGB32_FLOAT = 7

DXB_DEVICE_FLAG_DEBUG = 0x0001
DXB_DEVICE_FLAG_GPU_VALIDATION = 0x0002

DXB_BUFFER_FLAG_VERTEX = 0x0001
DXB_BUFFER_FLAG_DYNAMIC = 0x0008

DXB_SHADER_STAGE_VERTEX = 0
DXB_SHADER_STAGE_PIXEL = 1

DXB_PRIM_TRIANGLELIST = 0

DXB_SEMANTIC_POSITION = 0
DXB_SEMANTIC_COLOR = 3

DXB_COMPILE_DEBUG = 0x0001

DXB_FEATURE_DX12 = 0x0001

DXB_CAPABILITY_BACKEND_AVAILABLE = 1
DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE = 2
DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE = 3
DXB_CAPABILITY_ADAPTER_COUNT = 4
DXB_CAPABILITY_ADAPTER_SOFTWARE = 5
DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL = 6

LOG_LEVEL_NAMES = {
    0: "INFO",
    1: "WARN",
    2: "ERROR",
}

DXBDevice = ctypes.c_uint64
DXBSwapChain = ctypes.c_uint64
DXBRenderTarget = ctypes.c_uint64
DXBRTV = ctypes.c_uint64
DXBDSV = ctypes.c_uint64
DXBBuffer = ctypes.c_uint64
DXBShader = ctypes.c_uint64
DXBPipeline = ctypes.c_uint64
DXBInputLayout = ctypes.c_uint64

DXBLogCallback = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p)


class DXBAdapterInfo(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("index", ctypes.c_uint32),
        ("description", ctypes.c_char * 128),
        ("dedicated_video_mem", ctypes.c_uint64),
        ("dedicated_system_mem", ctypes.c_uint64),
        ("shared_system_mem", ctypes.c_uint64),
        ("is_software", ctypes.c_uint32),
    ]


class DXBDeviceDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("backend", ctypes.c_uint32),
        ("adapter_index", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
    ]


class DXBSwapChainDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("hwnd", ctypes.c_void_p),
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
        ("format", ctypes.c_uint32),
        ("buffer_count", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
    ]


class DXBBufferDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("byte_size", ctypes.c_uint32),
        ("stride", ctypes.c_uint32),
    ]


class DXBShaderDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("stage", ctypes.c_uint32),
        ("source_code", ctypes.c_char_p),
        ("source_size", ctypes.c_uint32),
        ("source_name", ctypes.c_char_p),
        ("entry_point", ctypes.c_char_p),
        ("target", ctypes.c_char_p),
        ("compile_flags", ctypes.c_uint32),
    ]


class DXBInputElementDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("semantic", ctypes.c_uint32),
        ("semantic_index", ctypes.c_uint32),
        ("format", ctypes.c_uint32),
        ("input_slot", ctypes.c_uint32),
        ("byte_offset", ctypes.c_uint32),
    ]


class DXBPipelineDesc(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("vs", DXBShader),
        ("ps", DXBShader),
        ("input_layout", DXBInputLayout),
        ("topology", ctypes.c_uint32),
        ("depth_test", ctypes.c_uint32),
        ("depth_write", ctypes.c_uint32),
        ("cull_back", ctypes.c_uint32),
        ("wireframe", ctypes.c_uint32),
    ]


class DXBViewport(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("width", ctypes.c_float),
        ("height", ctypes.c_float),
        ("min_depth", ctypes.c_float),
        ("max_depth", ctypes.c_float),
    ]


class DXBCapabilityQuery(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("capability", ctypes.c_uint32),
        ("backend", ctypes.c_uint32),
        ("adapter_index", ctypes.c_uint32),
        ("format", ctypes.c_uint32),
        ("_padding", ctypes.c_uint32),
        ("device", DXBDevice),
        ("reserved", ctypes.c_uint32 * 4),
    ]


class DXBCapabilityInfo(ctypes.Structure):
    _fields_ = [
        ("struct_version", ctypes.c_uint32),
        ("capability", ctypes.c_uint32),
        ("backend", ctypes.c_uint32),
        ("adapter_index", ctypes.c_uint32),
        ("supported", ctypes.c_uint32),
        ("value_u32", ctypes.c_uint32),
        ("value_u64", ctypes.c_uint64),
        ("reserved", ctypes.c_uint32 * 4),
    ]


def parse_version(version: int) -> str:
    major = (version >> 16) & 0xFF
    minor = (version >> 8) & 0xFF
    patch = version & 0xFF
    return f"{major}.{minor}.{patch}"


def format_hresult(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08X}"


def format_bytes(value: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    amount = float(value)
    unit = units[0]
    for unit in units:
        if amount < 1024.0 or unit == units[-1]:
            break
        amount /= 1024.0
    if unit == "B":
        return f"{int(amount)} {unit}"
    return f"{amount:.1f} {unit}"


def format_feature_level(value: int) -> str:
    if value == 0:
        return "n/a"
    major = (value >> 12) & 0xF
    minor = (value >> 8) & 0xF
    if major == 0 and minor == 0:
        return format_hresult(value)
    return f"{major}_{minor}"


def make_versioned(struct_type, **values):
    instance = struct_type()
    if hasattr(instance, "struct_version"):
        instance.struct_version = DXBRIDGE_STRUCT_VERSION
    for name, value in values.items():
        setattr(instance, name, value)
    return instance


def locate_dxbridge_dll(explicit_path: str | None = None) -> Path:
    candidates: list[Path] = []
    if explicit_path:
        candidates.append(Path(explicit_path))

    env_path = os.environ.get("DXBRIDGE_DLL")
    if env_path:
        candidates.append(Path(env_path))

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parents[1]
    candidates.extend(
        [
            Path.cwd() / "dxbridge.dll",
            script_dir / "dxbridge.dll",
            repo_root / "out" / "build" / "debug" / "Debug" / "dxbridge.dll",
            repo_root
            / "out"
            / "build"
            / "debug"
            / "examples"
            / "Debug"
            / "dxbridge.dll",
            repo_root / "out" / "build" / "debug" / "tests" / "Debug" / "dxbridge.dll",
            repo_root / "out" / "build" / "release" / "Release" / "dxbridge.dll",
            repo_root
            / "out"
            / "build"
            / "release"
            / "examples"
            / "Release"
            / "dxbridge.dll",
            repo_root
            / "out"
            / "build"
            / "release"
            / "tests"
            / "Release"
            / "dxbridge.dll",
            repo_root / "out" / "build" / "ci" / "Release" / "dxbridge.dll",
        ]
    )

    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file():
            return resolved

    searched = "\n  - ".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "Could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\n"
        f"Searched:\n  - {searched}"
    )


class DXBridgeError(RuntimeError):
    pass


class DXBridgeLibrary:
    def __init__(self, dll_path: str | None = None):
        self.path = locate_dxbridge_dll(dll_path)
        self.dll = ctypes.CDLL(str(self.path))
        self._callback_ref = None
        self._configure_prototypes()

    def _configure_prototypes(self) -> None:
        self.dll.DXBridge_SetLogCallback.argtypes = [DXBLogCallback, ctypes.c_void_p]
        self.dll.DXBridge_SetLogCallback.restype = None

        self.dll.DXBridge_Init.argtypes = [ctypes.c_uint32]
        self.dll.DXBridge_Init.restype = ctypes.c_int32

        self.dll.DXBridge_Shutdown.argtypes = []
        self.dll.DXBridge_Shutdown.restype = None

        self.dll.DXBridge_GetVersion.argtypes = []
        self.dll.DXBridge_GetVersion.restype = ctypes.c_uint32

        self.dll.DXBridge_SupportsFeature.argtypes = [ctypes.c_uint32]
        self.dll.DXBridge_SupportsFeature.restype = ctypes.c_uint32

        self.dll.DXBridge_QueryCapability.argtypes = [
            ctypes.POINTER(DXBCapabilityQuery),
            ctypes.POINTER(DXBCapabilityInfo),
        ]
        self.dll.DXBridge_QueryCapability.restype = ctypes.c_int32

        self.dll.DXBridge_GetLastError.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self.dll.DXBridge_GetLastError.restype = None

        self.dll.DXBridge_GetLastHRESULT.argtypes = []
        self.dll.DXBridge_GetLastHRESULT.restype = ctypes.c_uint32

        self.dll.DXBridge_EnumerateAdapters.argtypes = [
            ctypes.POINTER(DXBAdapterInfo),
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.DXBridge_EnumerateAdapters.restype = ctypes.c_int32

        self.dll.DXBridge_CreateDevice.argtypes = [
            ctypes.POINTER(DXBDeviceDesc),
            ctypes.POINTER(DXBDevice),
        ]
        self.dll.DXBridge_CreateDevice.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyDevice.argtypes = [DXBDevice]
        self.dll.DXBridge_DestroyDevice.restype = None

        self.dll.DXBridge_GetFeatureLevel.argtypes = [DXBDevice]
        self.dll.DXBridge_GetFeatureLevel.restype = ctypes.c_uint32

        self.dll.DXBridge_CreateSwapChain.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBSwapChainDesc),
            ctypes.POINTER(DXBSwapChain),
        ]
        self.dll.DXBridge_CreateSwapChain.restype = ctypes.c_int32

        self.dll.DXBridge_DestroySwapChain.argtypes = [DXBSwapChain]
        self.dll.DXBridge_DestroySwapChain.restype = None

        self.dll.DXBridge_ResizeSwapChain.argtypes = [
            DXBSwapChain,
            ctypes.c_uint32,
            ctypes.c_uint32,
        ]
        self.dll.DXBridge_ResizeSwapChain.restype = ctypes.c_int32

        self.dll.DXBridge_GetBackBuffer.argtypes = [
            DXBSwapChain,
            ctypes.POINTER(DXBRenderTarget),
        ]
        self.dll.DXBridge_GetBackBuffer.restype = ctypes.c_int32

        self.dll.DXBridge_CreateRenderTargetView.argtypes = [
            DXBDevice,
            DXBRenderTarget,
            ctypes.POINTER(DXBRTV),
        ]
        self.dll.DXBridge_CreateRenderTargetView.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyRTV.argtypes = [DXBRTV]
        self.dll.DXBridge_DestroyRTV.restype = None

        self.dll.DXBridge_CreateDepthStencilView.argtypes = [
            DXBDevice,
            ctypes.c_void_p,
            ctypes.POINTER(DXBDSV),
        ]
        self.dll.DXBridge_CreateDepthStencilView.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyDSV.argtypes = [DXBDSV]
        self.dll.DXBridge_DestroyDSV.restype = None

        self.dll.DXBridge_CompileShader.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBShaderDesc),
            ctypes.POINTER(DXBShader),
        ]
        self.dll.DXBridge_CompileShader.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyShader.argtypes = [DXBShader]
        self.dll.DXBridge_DestroyShader.restype = None

        self.dll.DXBridge_CreateInputLayout.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBInputElementDesc),
            ctypes.c_uint32,
            DXBShader,
            ctypes.POINTER(DXBInputLayout),
        ]
        self.dll.DXBridge_CreateInputLayout.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyInputLayout.argtypes = [DXBInputLayout]
        self.dll.DXBridge_DestroyInputLayout.restype = None

        self.dll.DXBridge_CreatePipelineState.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBPipelineDesc),
            ctypes.POINTER(DXBPipeline),
        ]
        self.dll.DXBridge_CreatePipelineState.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyPipeline.argtypes = [DXBPipeline]
        self.dll.DXBridge_DestroyPipeline.restype = None

        self.dll.DXBridge_CreateBuffer.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBBufferDesc),
            ctypes.POINTER(DXBBuffer),
        ]
        self.dll.DXBridge_CreateBuffer.restype = ctypes.c_int32

        self.dll.DXBridge_UploadData.argtypes = [
            DXBBuffer,
            ctypes.c_void_p,
            ctypes.c_uint32,
        ]
        self.dll.DXBridge_UploadData.restype = ctypes.c_int32

        self.dll.DXBridge_DestroyBuffer.argtypes = [DXBBuffer]
        self.dll.DXBridge_DestroyBuffer.restype = None

        self.dll.DXBridge_BeginFrame.argtypes = [DXBDevice]
        self.dll.DXBridge_BeginFrame.restype = ctypes.c_int32

        self.dll.DXBridge_SetRenderTargets.argtypes = [DXBDevice, DXBRTV, DXBDSV]
        self.dll.DXBridge_SetRenderTargets.restype = ctypes.c_int32

        self.dll.DXBridge_ClearRenderTarget.argtypes = [
            DXBRTV,
            ctypes.POINTER(ctypes.c_float),
        ]
        self.dll.DXBridge_ClearRenderTarget.restype = ctypes.c_int32

        self.dll.DXBridge_ClearDepthStencil.argtypes = [
            DXBDSV,
            ctypes.c_float,
            ctypes.c_uint8,
        ]
        self.dll.DXBridge_ClearDepthStencil.restype = ctypes.c_int32

        self.dll.DXBridge_SetViewport.argtypes = [
            DXBDevice,
            ctypes.POINTER(DXBViewport),
        ]
        self.dll.DXBridge_SetViewport.restype = ctypes.c_int32

        self.dll.DXBridge_SetPipeline.argtypes = [DXBDevice, DXBPipeline]
        self.dll.DXBridge_SetPipeline.restype = ctypes.c_int32

        self.dll.DXBridge_SetVertexBuffer.argtypes = [
            DXBDevice,
            DXBBuffer,
            ctypes.c_uint32,
            ctypes.c_uint32,
        ]
        self.dll.DXBridge_SetVertexBuffer.restype = ctypes.c_int32

        self.dll.DXBridge_Draw.argtypes = [
            DXBDevice,
            ctypes.c_uint32,
            ctypes.c_uint32,
        ]
        self.dll.DXBridge_Draw.restype = ctypes.c_int32

        self.dll.DXBridge_EndFrame.argtypes = [DXBDevice]
        self.dll.DXBridge_EndFrame.restype = ctypes.c_int32

        self.dll.DXBridge_Present.argtypes = [DXBSwapChain, ctypes.c_uint32]
        self.dll.DXBridge_Present.restype = ctypes.c_int32

    def get_last_error(self) -> str:
        buffer = ctypes.create_string_buffer(512)
        self.dll.DXBridge_GetLastError(buffer, len(buffer))
        return buffer.value.decode("utf-8", errors="replace")

    def get_last_hresult(self) -> int:
        return int(self.dll.DXBridge_GetLastHRESULT())

    def describe_failure(self, result: int) -> str:
        name = RESULT_NAMES.get(int(result), f"DXBResult({int(result)})")
        message = self.get_last_error()
        hresult = format_hresult(self.get_last_hresult())
        if message:
            return f"{name}: {message} (HRESULT {hresult})"
        return f"{name} (HRESULT {hresult})"

    def raise_for_result(self, result: int, function_name: str) -> None:
        if int(result) != DXB_OK:
            raise DXBridgeError(
                f"{function_name} failed: {self.describe_failure(result)}"
            )

    def set_log_callback(self, callback) -> None:
        if callback is None:
            self._callback_ref = None
            self.dll.DXBridge_SetLogCallback(DXBLogCallback(), None)
            return

        def bridge(level: int, msg: bytes, user_data) -> None:
            text = msg.decode("utf-8", errors="replace") if msg else ""
            callback(level, text, user_data)

        self._callback_ref = DXBLogCallback(bridge)
        self.dll.DXBridge_SetLogCallback(self._callback_ref, None)

    def get_version(self) -> int:
        return int(self.dll.DXBridge_GetVersion())

    def init(self, backend: int) -> None:
        self.raise_for_result(self.dll.DXBridge_Init(backend), "DXBridge_Init")

    def supports_feature(self, feature_flag: int) -> bool:
        return bool(self.dll.DXBridge_SupportsFeature(feature_flag))

    def query_capability(
        self,
        capability: int,
        backend: int,
        *,
        adapter_index: int = 0,
        format_code: int = DXB_FORMAT_UNKNOWN,
        device: int = DXBRIDGE_NULL_HANDLE,
    ) -> DXBCapabilityInfo:
        query = make_versioned(
            DXBCapabilityQuery,
            capability=capability,
            backend=backend,
            adapter_index=adapter_index,
            format=format_code,
            device=DXBDevice(device),
        )
        info = make_versioned(DXBCapabilityInfo)
        self.raise_for_result(
            self.dll.DXBridge_QueryCapability(ctypes.byref(query), ctypes.byref(info)),
            "DXBridge_QueryCapability",
        )
        return info

    def query_backend_available(self, backend: int) -> DXBCapabilityInfo:
        return self.query_capability(DXB_CAPABILITY_BACKEND_AVAILABLE, backend)

    def query_debug_layer_available(self, backend: int) -> DXBCapabilityInfo:
        return self.query_capability(DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE, backend)

    def query_gpu_validation_available(self, backend: int) -> DXBCapabilityInfo:
        return self.query_capability(DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE, backend)

    def query_adapter_count(self, backend: int) -> DXBCapabilityInfo:
        return self.query_capability(DXB_CAPABILITY_ADAPTER_COUNT, backend)

    def query_adapter_software(
        self, backend: int, adapter_index: int
    ) -> DXBCapabilityInfo:
        return self.query_capability(
            DXB_CAPABILITY_ADAPTER_SOFTWARE,
            backend,
            adapter_index=adapter_index,
        )

    def query_adapter_max_feature_level(
        self, backend: int, adapter_index: int
    ) -> DXBCapabilityInfo:
        return self.query_capability(
            DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL,
            backend,
            adapter_index=adapter_index,
        )

    def shutdown(self) -> None:
        self.dll.DXBridge_Shutdown()

    def enumerate_adapters(self) -> list[DXBAdapterInfo]:
        count = ctypes.c_uint32(0)
        self.raise_for_result(
            self.dll.DXBridge_EnumerateAdapters(None, ctypes.byref(count)),
            "DXBridge_EnumerateAdapters(count)",
        )
        if count.value == 0:
            return []

        adapters = (DXBAdapterInfo * count.value)()
        for adapter in adapters:
            adapter.struct_version = DXBRIDGE_STRUCT_VERSION

        self.raise_for_result(
            self.dll.DXBridge_EnumerateAdapters(adapters, ctypes.byref(count)),
            "DXBridge_EnumerateAdapters(fill)",
        )
        return list(adapters[: count.value])

    def create_device(
        self, backend: int, adapter_index: int = 0, flags: int = 0
    ) -> int:
        desc = make_versioned(
            DXBDeviceDesc,
            backend=backend,
            adapter_index=adapter_index,
            flags=flags,
        )
        handle = DXBDevice(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreateDevice(ctypes.byref(desc), ctypes.byref(handle)),
            "DXBridge_CreateDevice",
        )
        return int(handle.value)

    def destroy_device(self, device: int) -> None:
        self.dll.DXBridge_DestroyDevice(DXBDevice(device))

    def get_feature_level(self, device: int) -> int:
        return int(self.dll.DXBridge_GetFeatureLevel(DXBDevice(device)))

    def create_swapchain(
        self,
        device: int,
        hwnd: int,
        width: int,
        height: int,
        *,
        format_code: int = DXB_FORMAT_RGBA8_UNORM,
        buffer_count: int = 2,
        flags: int = 0,
    ) -> int:
        desc = make_versioned(
            DXBSwapChainDesc,
            hwnd=ctypes.c_void_p(hwnd),
            width=width,
            height=height,
            format=format_code,
            buffer_count=buffer_count,
            flags=flags,
        )
        handle = DXBSwapChain(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreateSwapChain(
                DXBDevice(device),
                ctypes.byref(desc),
                ctypes.byref(handle),
            ),
            "DXBridge_CreateSwapChain",
        )
        return int(handle.value)

    def destroy_swapchain(self, swapchain: int) -> None:
        self.dll.DXBridge_DestroySwapChain(DXBSwapChain(swapchain))

    def resize_swapchain(self, swapchain: int, width: int, height: int) -> None:
        self.raise_for_result(
            self.dll.DXBridge_ResizeSwapChain(
                DXBSwapChain(swapchain),
                ctypes.c_uint32(width),
                ctypes.c_uint32(height),
            ),
            "DXBridge_ResizeSwapChain",
        )

    def get_back_buffer(self, swapchain: int) -> int:
        handle = DXBRenderTarget(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_GetBackBuffer(
                DXBSwapChain(swapchain), ctypes.byref(handle)
            ),
            "DXBridge_GetBackBuffer",
        )
        return int(handle.value)

    def create_rtv(self, device: int, render_target: int) -> int:
        handle = DXBRTV(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreateRenderTargetView(
                DXBDevice(device),
                DXBRenderTarget(render_target),
                ctypes.byref(handle),
            ),
            "DXBridge_CreateRenderTargetView",
        )
        return int(handle.value)

    def destroy_rtv(self, rtv: int) -> None:
        self.dll.DXBridge_DestroyRTV(DXBRTV(rtv))

    def compile_shader(
        self,
        device: int,
        stage: int,
        source_code: str,
        source_name: str,
        entry_point: str,
        target: str,
        compile_flags: int = 0,
    ) -> int:
        source_bytes = source_code.encode("utf-8")
        desc = make_versioned(
            DXBShaderDesc,
            stage=stage,
            source_code=ctypes.c_char_p(source_bytes),
            source_size=0,
            source_name=ctypes.c_char_p(source_name.encode("utf-8")),
            entry_point=ctypes.c_char_p(entry_point.encode("utf-8")),
            target=ctypes.c_char_p(target.encode("utf-8")),
            compile_flags=compile_flags,
        )
        handle = DXBShader(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CompileShader(
                DXBDevice(device), ctypes.byref(desc), ctypes.byref(handle)
            ),
            "DXBridge_CompileShader",
        )
        return int(handle.value)

    def destroy_shader(self, shader: int) -> None:
        self.dll.DXBridge_DestroyShader(DXBShader(shader))

    def create_input_layout(
        self,
        device: int,
        elements: list[DXBInputElementDesc],
        vertex_shader: int,
    ) -> int:
        element_array = (DXBInputElementDesc * len(elements))(*elements)
        handle = DXBInputLayout(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreateInputLayout(
                DXBDevice(device),
                element_array,
                ctypes.c_uint32(len(elements)),
                DXBShader(vertex_shader),
                ctypes.byref(handle),
            ),
            "DXBridge_CreateInputLayout",
        )
        return int(handle.value)

    def destroy_input_layout(self, input_layout: int) -> None:
        self.dll.DXBridge_DestroyInputLayout(DXBInputLayout(input_layout))

    def create_pipeline_state(
        self,
        device: int,
        vertex_shader: int,
        pixel_shader: int,
        input_layout: int,
        *,
        topology: int = DXB_PRIM_TRIANGLELIST,
        depth_test: int = 0,
        depth_write: int = 0,
        cull_back: int = 0,
        wireframe: int = 0,
    ) -> int:
        desc = make_versioned(
            DXBPipelineDesc,
            vs=DXBShader(vertex_shader),
            ps=DXBShader(pixel_shader),
            input_layout=DXBInputLayout(input_layout),
            topology=topology,
            depth_test=depth_test,
            depth_write=depth_write,
            cull_back=cull_back,
            wireframe=wireframe,
        )
        handle = DXBPipeline(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreatePipelineState(
                DXBDevice(device), ctypes.byref(desc), ctypes.byref(handle)
            ),
            "DXBridge_CreatePipelineState",
        )
        return int(handle.value)

    def destroy_pipeline(self, pipeline: int) -> None:
        self.dll.DXBridge_DestroyPipeline(DXBPipeline(pipeline))

    def create_buffer(
        self,
        device: int,
        flags: int,
        byte_size: int,
        stride: int,
    ) -> int:
        desc = make_versioned(
            DXBBufferDesc,
            flags=flags,
            byte_size=byte_size,
            stride=stride,
        )
        handle = DXBBuffer(DXBRIDGE_NULL_HANDLE)
        self.raise_for_result(
            self.dll.DXBridge_CreateBuffer(
                DXBDevice(device), ctypes.byref(desc), ctypes.byref(handle)
            ),
            "DXBridge_CreateBuffer",
        )
        return int(handle.value)

    def upload_data(self, buffer: int, data, byte_size: int | None = None) -> None:
        if isinstance(data, (bytes, bytearray, memoryview)):
            raw = ctypes.create_string_buffer(bytes(data))
            ptr = ctypes.cast(raw, ctypes.c_void_p)
            size = byte_size if byte_size is not None else len(raw)
        else:
            ptr = ctypes.cast(data, ctypes.c_void_p)
            size = byte_size if byte_size is not None else ctypes.sizeof(data)

        self.raise_for_result(
            self.dll.DXBridge_UploadData(DXBBuffer(buffer), ptr, ctypes.c_uint32(size)),
            "DXBridge_UploadData",
        )

    def destroy_buffer(self, buffer: int) -> None:
        self.dll.DXBridge_DestroyBuffer(DXBBuffer(buffer))

    def begin_frame(self, device: int) -> None:
        self.raise_for_result(
            self.dll.DXBridge_BeginFrame(DXBDevice(device)), "DXBridge_BeginFrame"
        )

    def set_render_targets(
        self, device: int, rtv: int, dsv: int = DXBRIDGE_NULL_HANDLE
    ) -> None:
        self.raise_for_result(
            self.dll.DXBridge_SetRenderTargets(
                DXBDevice(device), DXBRTV(rtv), DXBDSV(dsv)
            ),
            "DXBridge_SetRenderTargets",
        )

    def clear_render_target(self, rtv: int, rgba) -> None:
        values = (ctypes.c_float * 4)(*rgba)
        self.raise_for_result(
            self.dll.DXBridge_ClearRenderTarget(DXBRTV(rtv), values),
            "DXBridge_ClearRenderTarget",
        )

    def clear_depth_stencil(self, dsv: int, depth: float, stencil: int = 0) -> None:
        self.raise_for_result(
            self.dll.DXBridge_ClearDepthStencil(
                DXBDSV(dsv), ctypes.c_float(depth), ctypes.c_uint8(stencil)
            ),
            "DXBridge_ClearDepthStencil",
        )

    def set_viewport(
        self,
        device: int,
        x: float,
        y: float,
        width: float,
        height: float,
        min_depth: float = 0.0,
        max_depth: float = 1.0,
    ) -> None:
        viewport = DXBViewport(
            x=ctypes.c_float(x),
            y=ctypes.c_float(y),
            width=ctypes.c_float(width),
            height=ctypes.c_float(height),
            min_depth=ctypes.c_float(min_depth),
            max_depth=ctypes.c_float(max_depth),
        )
        self.raise_for_result(
            self.dll.DXBridge_SetViewport(DXBDevice(device), ctypes.byref(viewport)),
            "DXBridge_SetViewport",
        )

    def set_pipeline(self, device: int, pipeline: int) -> None:
        self.raise_for_result(
            self.dll.DXBridge_SetPipeline(DXBDevice(device), DXBPipeline(pipeline)),
            "DXBridge_SetPipeline",
        )

    def set_vertex_buffer(
        self, device: int, buffer: int, stride: int, offset: int = 0
    ) -> None:
        self.raise_for_result(
            self.dll.DXBridge_SetVertexBuffer(
                DXBDevice(device),
                DXBBuffer(buffer),
                ctypes.c_uint32(stride),
                ctypes.c_uint32(offset),
            ),
            "DXBridge_SetVertexBuffer",
        )

    def draw(self, device: int, vertex_count: int, start_vertex: int = 0) -> None:
        self.raise_for_result(
            self.dll.DXBridge_Draw(
                DXBDevice(device),
                ctypes.c_uint32(vertex_count),
                ctypes.c_uint32(start_vertex),
            ),
            "DXBridge_Draw",
        )

    def end_frame(self, device: int) -> None:
        self.raise_for_result(
            self.dll.DXBridge_EndFrame(DXBDevice(device)), "DXBridge_EndFrame"
        )

    def present(self, swapchain: int, sync_interval: int = 1) -> None:
        self.raise_for_result(
            self.dll.DXBridge_Present(DXBSwapChain(swapchain), sync_interval),
            "DXBridge_Present",
        )
