#![cfg(windows)]

use libloading::Library;
use std::error::Error;
use std::ffi::{c_char, c_void, CStr, CString};
use std::fmt;
use std::path::{Path, PathBuf};
use std::ptr;
use std::sync::{Mutex, OnceLock};

pub type AnyResult<T> = Result<T, Box<dyn Error>>;

pub const DXBRIDGE_STRUCT_VERSION: u32 = 1;
pub const DXB_NULL_HANDLE: u64 = 0;

pub const DXB_OK: i32 = 0;
pub const DXB_E_INVALID_HANDLE: i32 = -1;
pub const DXB_E_DEVICE_LOST: i32 = -2;
pub const DXB_E_OUT_OF_MEMORY: i32 = -3;
pub const DXB_E_INVALID_ARG: i32 = -4;
pub const DXB_E_NOT_SUPPORTED: i32 = -5;
pub const DXB_E_SHADER_COMPILE: i32 = -6;
pub const DXB_E_NOT_INITIALIZED: i32 = -7;
pub const DXB_E_INVALID_STATE: i32 = -8;
pub const DXB_E_INTERNAL: i32 = -999;

pub const DXB_BACKEND_AUTO: u32 = 0;
pub const DXB_BACKEND_DX11: u32 = 1;
pub const DXB_BACKEND_DX12: u32 = 2;

pub const DXB_FORMAT_UNKNOWN: u32 = 0;
pub const DXB_FORMAT_RGBA8_UNORM: u32 = 1;
pub const DXB_FORMAT_D32_FLOAT: u32 = 3;
pub const DXB_FORMAT_RG32_FLOAT: u32 = 6;
pub const DXB_FORMAT_RGB32_FLOAT: u32 = 7;

pub const DXB_DEVICE_FLAG_DEBUG: u32 = 0x0001;
pub const DXB_DEVICE_FLAG_GPU_VALIDATION: u32 = 0x0002;

pub const DXB_BUFFER_FLAG_VERTEX: u32 = 0x0001;
pub const DXB_BUFFER_FLAG_DYNAMIC: u32 = 0x0008;

pub const DXB_SHADER_STAGE_VERTEX: u32 = 0;
pub const DXB_SHADER_STAGE_PIXEL: u32 = 1;

pub const DXB_PRIM_TRIANGLELIST: u32 = 0;

pub const DXB_SEMANTIC_POSITION: u32 = 0;
pub const DXB_SEMANTIC_COLOR: u32 = 3;

pub const DXB_COMPILE_DEBUG: u32 = 0x0001;
pub const DXB_FEATURE_DX12: u32 = 0x0001;

pub const DXB_CAPABILITY_BACKEND_AVAILABLE: u32 = 1;
pub const DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE: u32 = 2;
pub const DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE: u32 = 3;
pub const DXB_CAPABILITY_ADAPTER_COUNT: u32 = 4;
pub const DXB_CAPABILITY_ADAPTER_SOFTWARE: u32 = 5;
pub const DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL: u32 = 6;

pub type DxbResult = i32;
pub type DxbDevice = u64;
pub type DxbSwapChain = u64;
pub type DxbBuffer = u64;
pub type DxbShader = u64;
pub type DxbPipeline = u64;
pub type DxbInputLayout = u64;
pub type DxbRtv = u64;
pub type DxbRenderTarget = u64;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DxbAdapterInfo {
    pub struct_version: u32,
    pub index: u32,
    pub description: [u8; 128],
    pub dedicated_video_mem: u64,
    pub dedicated_system_mem: u64,
    pub shared_system_mem: u64,
    pub is_software: u32,
}

impl Default for DxbAdapterInfo {
    fn default() -> Self {
        Self {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            index: 0,
            description: [0; 128],
            dedicated_video_mem: 0,
            dedicated_system_mem: 0,
            shared_system_mem: 0,
            is_software: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbDeviceDesc {
    pub struct_version: u32,
    pub backend: u32,
    pub adapter_index: u32,
    pub flags: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbSwapChainDesc {
    pub struct_version: u32,
    pub hwnd: *mut c_void,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub buffer_count: u32,
    pub flags: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbBufferDesc {
    pub struct_version: u32,
    pub flags: u32,
    pub byte_size: u32,
    pub stride: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbShaderDesc {
    pub struct_version: u32,
    pub stage: u32,
    pub source_code: *const c_char,
    pub source_size: u32,
    pub source_name: *const c_char,
    pub entry_point: *const c_char,
    pub target: *const c_char,
    pub compile_flags: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbInputElementDesc {
    pub struct_version: u32,
    pub semantic: u32,
    pub semantic_index: u32,
    pub format: u32,
    pub input_slot: u32,
    pub byte_offset: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbPipelineDesc {
    pub struct_version: u32,
    pub vs: DxbShader,
    pub ps: DxbShader,
    pub input_layout: DxbInputLayout,
    pub topology: u32,
    pub depth_test: u32,
    pub depth_write: u32,
    pub cull_back: u32,
    pub wireframe: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbViewport {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub min_depth: f32,
    pub max_depth: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbCapabilityQuery {
    pub struct_version: u32,
    pub capability: u32,
    pub backend: u32,
    pub adapter_index: u32,
    pub format: u32,
    pub _padding: u32,
    pub device: DxbDevice,
    pub reserved: [u32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DxbCapabilityInfo {
    pub struct_version: u32,
    pub capability: u32,
    pub backend: u32,
    pub adapter_index: u32,
    pub supported: u32,
    pub value_u32: u32,
    pub value_u64: u64,
    pub reserved: [u32; 4],
}

type DxbLogCallback =
    unsafe extern "C" fn(level: i32, message: *const c_char, user_data: *mut c_void);
type DxBridgeSetLogCallback =
    unsafe extern "C" fn(cb: Option<DxbLogCallback>, user_data: *mut c_void);
type DxBridgeInit = unsafe extern "C" fn(backend_hint: u32) -> DxbResult;
type DxBridgeShutdown = unsafe extern "C" fn();
type DxBridgeGetVersion = unsafe extern "C" fn() -> u32;
type DxBridgeSupportsFeature = unsafe extern "C" fn(feature_flag: u32) -> u32;
type DxBridgeQueryCapability = unsafe extern "C" fn(
    query: *const DxbCapabilityQuery,
    out_info: *mut DxbCapabilityInfo,
) -> DxbResult;
type DxBridgeGetLastError = unsafe extern "C" fn(buf: *mut c_char, buf_size: i32);
type DxBridgeGetLastHresult = unsafe extern "C" fn() -> u32;
type DxBridgeEnumerateAdapters =
    unsafe extern "C" fn(out_buf: *mut DxbAdapterInfo, in_out_count: *mut u32) -> DxbResult;
type DxBridgeCreateDevice =
    unsafe extern "C" fn(desc: *const DxbDeviceDesc, out_device: *mut DxbDevice) -> DxbResult;
type DxBridgeDestroyDevice = unsafe extern "C" fn(device: DxbDevice);
type DxBridgeGetFeatureLevel = unsafe extern "C" fn(device: DxbDevice) -> u32;
type DxBridgeCreateSwapChain = unsafe extern "C" fn(
    device: DxbDevice,
    desc: *const DxbSwapChainDesc,
    out_sc: *mut DxbSwapChain,
) -> DxbResult;
type DxBridgeDestroySwapChain = unsafe extern "C" fn(sc: DxbSwapChain);
type DxBridgeGetBackBuffer =
    unsafe extern "C" fn(sc: DxbSwapChain, out_rt: *mut DxbRenderTarget) -> DxbResult;
type DxBridgeCreateRenderTargetView =
    unsafe extern "C" fn(device: DxbDevice, rt: DxbRenderTarget, out_rtv: *mut DxbRtv) -> DxbResult;
type DxBridgeDestroyRtv = unsafe extern "C" fn(rtv: DxbRtv);
type DxBridgeCompileShader = unsafe extern "C" fn(
    device: DxbDevice,
    desc: *const DxbShaderDesc,
    out_shader: *mut DxbShader,
) -> DxbResult;
type DxBridgeDestroyShader = unsafe extern "C" fn(shader: DxbShader);
type DxBridgeCreateInputLayout = unsafe extern "C" fn(
    device: DxbDevice,
    elements: *const DxbInputElementDesc,
    element_count: u32,
    vs: DxbShader,
    out_layout: *mut DxbInputLayout,
) -> DxbResult;
type DxBridgeDestroyInputLayout = unsafe extern "C" fn(layout: DxbInputLayout);
type DxBridgeCreatePipelineState = unsafe extern "C" fn(
    device: DxbDevice,
    desc: *const DxbPipelineDesc,
    out_pipeline: *mut DxbPipeline,
) -> DxbResult;
type DxBridgeDestroyPipeline = unsafe extern "C" fn(pipeline: DxbPipeline);
type DxBridgeCreateBuffer = unsafe extern "C" fn(
    device: DxbDevice,
    desc: *const DxbBufferDesc,
    out_buf: *mut DxbBuffer,
) -> DxbResult;
type DxBridgeUploadData =
    unsafe extern "C" fn(buf: DxbBuffer, data: *const c_void, byte_size: u32) -> DxbResult;
type DxBridgeDestroyBuffer = unsafe extern "C" fn(buf: DxbBuffer);
type DxBridgeBeginFrame = unsafe extern "C" fn(device: DxbDevice) -> DxbResult;
type DxBridgeSetRenderTargets =
    unsafe extern "C" fn(device: DxbDevice, rtv: DxbRtv, dsv: u64) -> DxbResult;
type DxBridgeClearRenderTarget = unsafe extern "C" fn(rtv: DxbRtv, rgba: *const f32) -> DxbResult;
type DxBridgeSetViewport =
    unsafe extern "C" fn(device: DxbDevice, vp: *const DxbViewport) -> DxbResult;
type DxBridgeSetPipeline =
    unsafe extern "C" fn(device: DxbDevice, pipeline: DxbPipeline) -> DxbResult;
type DxBridgeSetVertexBuffer =
    unsafe extern "C" fn(device: DxbDevice, buf: DxbBuffer, stride: u32, offset: u32) -> DxbResult;
type DxBridgeDraw =
    unsafe extern "C" fn(device: DxbDevice, vertex_count: u32, start_vertex: u32) -> DxbResult;
type DxBridgeEndFrame = unsafe extern "C" fn(device: DxbDevice) -> DxbResult;
type DxBridgePresent = unsafe extern "C" fn(sc: DxbSwapChain, sync_interval: u32) -> DxbResult;

type LogHandler = dyn Fn(i32, &str) + Send + Sync + 'static;

static LOG_HANDLER: OnceLock<Mutex<Option<Box<LogHandler>>>> = OnceLock::new();

fn log_handler_slot() -> &'static Mutex<Option<Box<LogHandler>>> {
    LOG_HANDLER.get_or_init(|| Mutex::new(None))
}

unsafe extern "C" fn log_callback_trampoline(
    level: i32,
    message: *const c_char,
    _user_data: *mut c_void,
) {
    let text = if message.is_null() {
        ""
    } else {
        CStr::from_ptr(message)
            .to_str()
            .unwrap_or("<non-utf8 log message>")
    };

    if let Some(handler) = log_handler_slot().lock().unwrap().as_ref() {
        handler(level, text);
    }
}

pub struct DxBridge {
    _library: Library,
    pub path: PathBuf,
    set_log_callback: DxBridgeSetLogCallback,
    init: DxBridgeInit,
    shutdown: DxBridgeShutdown,
    get_version: DxBridgeGetVersion,
    supports_feature: DxBridgeSupportsFeature,
    query_capability: DxBridgeQueryCapability,
    get_last_error: DxBridgeGetLastError,
    get_last_hresult: DxBridgeGetLastHresult,
    enumerate_adapters: DxBridgeEnumerateAdapters,
    create_device: DxBridgeCreateDevice,
    destroy_device: DxBridgeDestroyDevice,
    get_feature_level: DxBridgeGetFeatureLevel,
    create_swap_chain: DxBridgeCreateSwapChain,
    destroy_swap_chain: DxBridgeDestroySwapChain,
    get_back_buffer: DxBridgeGetBackBuffer,
    create_render_target_view: DxBridgeCreateRenderTargetView,
    destroy_rtv: DxBridgeDestroyRtv,
    compile_shader: DxBridgeCompileShader,
    destroy_shader: DxBridgeDestroyShader,
    create_input_layout: DxBridgeCreateInputLayout,
    destroy_input_layout: DxBridgeDestroyInputLayout,
    create_pipeline_state: DxBridgeCreatePipelineState,
    destroy_pipeline: DxBridgeDestroyPipeline,
    create_buffer: DxBridgeCreateBuffer,
    upload_data: DxBridgeUploadData,
    destroy_buffer: DxBridgeDestroyBuffer,
    begin_frame: DxBridgeBeginFrame,
    set_render_targets: DxBridgeSetRenderTargets,
    clear_render_target: DxBridgeClearRenderTarget,
    set_viewport: DxBridgeSetViewport,
    set_pipeline: DxBridgeSetPipeline,
    set_vertex_buffer: DxBridgeSetVertexBuffer,
    draw: DxBridgeDraw,
    end_frame: DxBridgeEndFrame,
    present: DxBridgePresent,
}

impl fmt::Debug for DxBridge {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DxBridge")
            .field("path", &self.path)
            .finish()
    }
}

impl DxBridge {
    pub fn load(explicit_path: Option<&str>) -> AnyResult<Self> {
        let path = locate_dll(explicit_path)?;
        let library = unsafe { Library::new(&path)? };

        unsafe {
            Ok(Self {
                set_log_callback: *library.get(b"DXBridge_SetLogCallback\0")?,
                init: *library.get(b"DXBridge_Init\0")?,
                shutdown: *library.get(b"DXBridge_Shutdown\0")?,
                get_version: *library.get(b"DXBridge_GetVersion\0")?,
                supports_feature: *library.get(b"DXBridge_SupportsFeature\0")?,
                query_capability: *library.get(b"DXBridge_QueryCapability\0")?,
                get_last_error: *library.get(b"DXBridge_GetLastError\0")?,
                get_last_hresult: *library.get(b"DXBridge_GetLastHRESULT\0")?,
                enumerate_adapters: *library.get(b"DXBridge_EnumerateAdapters\0")?,
                create_device: *library.get(b"DXBridge_CreateDevice\0")?,
                destroy_device: *library.get(b"DXBridge_DestroyDevice\0")?,
                get_feature_level: *library.get(b"DXBridge_GetFeatureLevel\0")?,
                create_swap_chain: *library.get(b"DXBridge_CreateSwapChain\0")?,
                destroy_swap_chain: *library.get(b"DXBridge_DestroySwapChain\0")?,
                get_back_buffer: *library.get(b"DXBridge_GetBackBuffer\0")?,
                create_render_target_view: *library.get(b"DXBridge_CreateRenderTargetView\0")?,
                destroy_rtv: *library.get(b"DXBridge_DestroyRTV\0")?,
                compile_shader: *library.get(b"DXBridge_CompileShader\0")?,
                destroy_shader: *library.get(b"DXBridge_DestroyShader\0")?,
                create_input_layout: *library.get(b"DXBridge_CreateInputLayout\0")?,
                destroy_input_layout: *library.get(b"DXBridge_DestroyInputLayout\0")?,
                create_pipeline_state: *library.get(b"DXBridge_CreatePipelineState\0")?,
                destroy_pipeline: *library.get(b"DXBridge_DestroyPipeline\0")?,
                create_buffer: *library.get(b"DXBridge_CreateBuffer\0")?,
                upload_data: *library.get(b"DXBridge_UploadData\0")?,
                destroy_buffer: *library.get(b"DXBridge_DestroyBuffer\0")?,
                begin_frame: *library.get(b"DXBridge_BeginFrame\0")?,
                set_render_targets: *library.get(b"DXBridge_SetRenderTargets\0")?,
                clear_render_target: *library.get(b"DXBridge_ClearRenderTarget\0")?,
                set_viewport: *library.get(b"DXBridge_SetViewport\0")?,
                set_pipeline: *library.get(b"DXBridge_SetPipeline\0")?,
                set_vertex_buffer: *library.get(b"DXBridge_SetVertexBuffer\0")?,
                draw: *library.get(b"DXBridge_Draw\0")?,
                end_frame: *library.get(b"DXBridge_EndFrame\0")?,
                present: *library.get(b"DXBridge_Present\0")?,
                _library: library,
                path,
            })
        }
    }

    pub fn set_log_callback<F>(&self, handler: F)
    where
        F: Fn(i32, &str) + Send + Sync + 'static,
    {
        *log_handler_slot().lock().unwrap() = Some(Box::new(handler));
        unsafe {
            (self.set_log_callback)(Some(log_callback_trampoline), ptr::null_mut());
        }
    }

    pub fn clear_log_callback(&self) {
        *log_handler_slot().lock().unwrap() = None;
        unsafe {
            (self.set_log_callback)(None, ptr::null_mut());
        }
    }

    pub fn get_version(&self) -> u32 {
        unsafe { (self.get_version)() }
    }

    pub fn supports_feature(&self, feature_flag: u32) -> bool {
        unsafe { (self.supports_feature)(feature_flag) != 0 }
    }

    pub fn query_capability(
        &self,
        capability: u32,
        backend: u32,
        adapter_index: u32,
    ) -> AnyResult<DxbCapabilityInfo> {
        let query = DxbCapabilityQuery {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            capability,
            backend,
            adapter_index,
            format: DXB_FORMAT_UNKNOWN,
            _padding: 0,
            device: DXB_NULL_HANDLE,
            reserved: [0; 4],
        };
        let mut info = DxbCapabilityInfo {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            ..Default::default()
        };
        self.require_ok(
            unsafe { (self.query_capability)(&query, &mut info) },
            "DXBridge_QueryCapability",
        )?;
        Ok(info)
    }

    pub fn query_backend_available(&self, backend: u32) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(DXB_CAPABILITY_BACKEND_AVAILABLE, backend, 0)
    }

    pub fn query_debug_layer_available(&self, backend: u32) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE, backend, 0)
    }

    pub fn query_gpu_validation_available(&self, backend: u32) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE, backend, 0)
    }

    pub fn query_adapter_count(&self, backend: u32) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(DXB_CAPABILITY_ADAPTER_COUNT, backend, 0)
    }

    pub fn query_adapter_software(
        &self,
        backend: u32,
        adapter_index: u32,
    ) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(DXB_CAPABILITY_ADAPTER_SOFTWARE, backend, adapter_index)
    }

    pub fn query_adapter_max_feature_level(
        &self,
        backend: u32,
        adapter_index: u32,
    ) -> AnyResult<DxbCapabilityInfo> {
        self.query_capability(
            DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL,
            backend,
            adapter_index,
        )
    }

    pub fn init(&self, backend: u32) -> AnyResult<()> {
        self.require_ok(unsafe { (self.init)(backend) }, "DXBridge_Init")
    }

    pub fn shutdown(&self) {
        unsafe { (self.shutdown)() }
    }

    pub fn get_last_error(&self) -> String {
        let mut buffer = [0u8; 512];
        unsafe {
            (self.get_last_error)(buffer.as_mut_ptr().cast::<c_char>(), buffer.len() as i32);
        }
        let len = buffer
            .iter()
            .position(|byte| *byte == 0)
            .unwrap_or(buffer.len());
        String::from_utf8_lossy(&buffer[..len]).into_owned()
    }

    pub fn get_last_hresult(&self) -> u32 {
        unsafe { (self.get_last_hresult)() }
    }

    pub fn describe_failure(&self, result: DxbResult) -> String {
        let message = self.get_last_error();
        let hresult = format_hresult(self.get_last_hresult());
        let name = result_name(result);
        if message.is_empty() {
            format!("{} (HRESULT {})", name, hresult)
        } else {
            format!("{}: {} (HRESULT {})", name, message, hresult)
        }
    }

    pub fn enumerate_adapters(&self) -> AnyResult<Vec<DxbAdapterInfo>> {
        let mut count = 0u32;
        self.require_ok(
            unsafe { (self.enumerate_adapters)(ptr::null_mut(), &mut count) },
            "DXBridge_EnumerateAdapters(count)",
        )?;

        if count == 0 {
            return Ok(Vec::new());
        }

        let mut adapters = vec![DxbAdapterInfo::default(); count as usize];
        self.require_ok(
            unsafe { (self.enumerate_adapters)(adapters.as_mut_ptr(), &mut count) },
            "DXBridge_EnumerateAdapters(fill)",
        )?;
        adapters.truncate(count as usize);
        Ok(adapters)
    }

    pub unsafe fn create_device_raw(
        &self,
        desc: *const DxbDeviceDesc,
        out_device: *mut DxbDevice,
    ) -> DxbResult {
        (self.create_device)(desc, out_device)
    }

    pub fn create_device(
        &self,
        backend: u32,
        adapter_index: u32,
        flags: u32,
    ) -> AnyResult<DxbDevice> {
        let desc = DxbDeviceDesc {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            backend,
            adapter_index,
            flags,
        };
        let mut device = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.create_device)(&desc, &mut device) },
            "DXBridge_CreateDevice",
        )?;
        Ok(device)
    }

    pub fn destroy_device(&self, device: DxbDevice) {
        unsafe { (self.destroy_device)(device) }
    }

    pub fn get_feature_level(&self, device: DxbDevice) -> u32 {
        unsafe { (self.get_feature_level)(device) }
    }

    pub fn create_swap_chain(
        &self,
        device: DxbDevice,
        hwnd: *mut c_void,
        width: u32,
        height: u32,
        format: u32,
        buffer_count: u32,
        flags: u32,
    ) -> AnyResult<DxbSwapChain> {
        let desc = DxbSwapChainDesc {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            hwnd,
            width,
            height,
            format,
            buffer_count,
            flags,
        };
        let mut swap_chain = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.create_swap_chain)(device, &desc, &mut swap_chain) },
            "DXBridge_CreateSwapChain",
        )?;
        Ok(swap_chain)
    }

    pub fn destroy_swap_chain(&self, swap_chain: DxbSwapChain) {
        unsafe { (self.destroy_swap_chain)(swap_chain) }
    }

    pub fn get_back_buffer(&self, swap_chain: DxbSwapChain) -> AnyResult<DxbRenderTarget> {
        let mut render_target = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.get_back_buffer)(swap_chain, &mut render_target) },
            "DXBridge_GetBackBuffer",
        )?;
        Ok(render_target)
    }

    pub fn create_render_target_view(
        &self,
        device: DxbDevice,
        render_target: DxbRenderTarget,
    ) -> AnyResult<DxbRtv> {
        let mut rtv = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.create_render_target_view)(device, render_target, &mut rtv) },
            "DXBridge_CreateRenderTargetView",
        )?;
        Ok(rtv)
    }

    pub fn destroy_render_target_view(&self, rtv: DxbRtv) {
        unsafe { (self.destroy_rtv)(rtv) }
    }

    pub fn compile_shader(
        &self,
        device: DxbDevice,
        stage: u32,
        source_code: &str,
        source_name: &str,
        entry_point: &str,
        target: &str,
        compile_flags: u32,
    ) -> AnyResult<DxbShader> {
        let source_code = CString::new(source_code)?;
        let source_name = CString::new(source_name)?;
        let entry_point = CString::new(entry_point)?;
        let target = CString::new(target)?;
        let desc = DxbShaderDesc {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            stage,
            source_code: source_code.as_ptr(),
            source_size: 0,
            source_name: source_name.as_ptr(),
            entry_point: entry_point.as_ptr(),
            target: target.as_ptr(),
            compile_flags,
        };
        let mut shader = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.compile_shader)(device, &desc, &mut shader) },
            "DXBridge_CompileShader",
        )?;
        Ok(shader)
    }

    pub fn destroy_shader(&self, shader: DxbShader) {
        unsafe { (self.destroy_shader)(shader) }
    }

    pub fn create_input_layout(
        &self,
        device: DxbDevice,
        elements: &[DxbInputElementDesc],
        vertex_shader: DxbShader,
    ) -> AnyResult<DxbInputLayout> {
        if elements.is_empty() {
            return Err("elements must not be empty".into());
        }
        let mut layout = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe {
                (self.create_input_layout)(
                    device,
                    elements.as_ptr(),
                    elements.len() as u32,
                    vertex_shader,
                    &mut layout,
                )
            },
            "DXBridge_CreateInputLayout",
        )?;
        Ok(layout)
    }

    pub fn destroy_input_layout(&self, layout: DxbInputLayout) {
        unsafe { (self.destroy_input_layout)(layout) }
    }

    pub fn create_pipeline_state(
        &self,
        device: DxbDevice,
        vertex_shader: DxbShader,
        pixel_shader: DxbShader,
        input_layout: DxbInputLayout,
        topology: u32,
        depth_test: u32,
        depth_write: u32,
        cull_back: u32,
        wireframe: u32,
    ) -> AnyResult<DxbPipeline> {
        let desc = DxbPipelineDesc {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            vs: vertex_shader,
            ps: pixel_shader,
            input_layout,
            topology,
            depth_test,
            depth_write,
            cull_back,
            wireframe,
        };
        let mut pipeline = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.create_pipeline_state)(device, &desc, &mut pipeline) },
            "DXBridge_CreatePipelineState",
        )?;
        Ok(pipeline)
    }

    pub fn destroy_pipeline(&self, pipeline: DxbPipeline) {
        unsafe { (self.destroy_pipeline)(pipeline) }
    }

    pub fn create_buffer(
        &self,
        device: DxbDevice,
        flags: u32,
        byte_size: u32,
        stride: u32,
    ) -> AnyResult<DxbBuffer> {
        let desc = DxbBufferDesc {
            struct_version: DXBRIDGE_STRUCT_VERSION,
            flags,
            byte_size,
            stride,
        };
        let mut buffer = DXB_NULL_HANDLE;
        self.require_ok(
            unsafe { (self.create_buffer)(device, &desc, &mut buffer) },
            "DXBridge_CreateBuffer",
        )?;
        Ok(buffer)
    }

    pub fn upload_floats(&self, buffer: DxbBuffer, values: &[f32]) -> AnyResult<()> {
        if values.is_empty() {
            return Err("values must not be empty".into());
        }
        self.require_ok(
            unsafe {
                (self.upload_data)(
                    buffer,
                    values.as_ptr().cast::<c_void>(),
                    (values.len() * std::mem::size_of::<f32>()) as u32,
                )
            },
            "DXBridge_UploadData",
        )
    }

    pub fn destroy_buffer(&self, buffer: DxbBuffer) {
        unsafe { (self.destroy_buffer)(buffer) }
    }

    pub fn begin_frame(&self, device: DxbDevice) -> AnyResult<()> {
        self.require_ok(unsafe { (self.begin_frame)(device) }, "DXBridge_BeginFrame")
    }

    pub fn set_render_targets(&self, device: DxbDevice, rtv: DxbRtv, dsv: u64) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.set_render_targets)(device, rtv, dsv) },
            "DXBridge_SetRenderTargets",
        )
    }

    pub fn clear_render_target(&self, rtv: DxbRtv, rgba: [f32; 4]) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.clear_render_target)(rtv, rgba.as_ptr()) },
            "DXBridge_ClearRenderTarget",
        )
    }

    pub fn set_viewport(&self, device: DxbDevice, viewport: DxbViewport) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.set_viewport)(device, &viewport) },
            "DXBridge_SetViewport",
        )
    }

    pub fn set_pipeline(&self, device: DxbDevice, pipeline: DxbPipeline) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.set_pipeline)(device, pipeline) },
            "DXBridge_SetPipeline",
        )
    }

    pub fn set_vertex_buffer(
        &self,
        device: DxbDevice,
        buffer: DxbBuffer,
        stride: u32,
        offset: u32,
    ) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.set_vertex_buffer)(device, buffer, stride, offset) },
            "DXBridge_SetVertexBuffer",
        )
    }

    pub fn draw(&self, device: DxbDevice, vertex_count: u32, start_vertex: u32) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.draw)(device, vertex_count, start_vertex) },
            "DXBridge_Draw",
        )
    }

    pub fn end_frame(&self, device: DxbDevice) -> AnyResult<()> {
        self.require_ok(unsafe { (self.end_frame)(device) }, "DXBridge_EndFrame")
    }

    pub fn present(&self, swap_chain: DxbSwapChain, sync_interval: u32) -> AnyResult<()> {
        self.require_ok(
            unsafe { (self.present)(swap_chain, sync_interval) },
            "DXBridge_Present",
        )
    }

    fn require_ok(&self, result: DxbResult, function_name: &str) -> AnyResult<()> {
        if result == DXB_OK {
            Ok(())
        } else {
            Err(format!(
                "{} failed: {}",
                function_name,
                self.describe_failure(result)
            )
            .into())
        }
    }
}

impl Drop for DxBridge {
    fn drop(&mut self) {
        self.clear_log_callback();
    }
}

pub fn parse_version(version: u32) -> String {
    let major = (version >> 16) & 0xFF;
    let minor = (version >> 8) & 0xFF;
    let patch = version & 0xFF;
    format!("{}.{}.{}", major, minor, patch)
}

pub fn parse_backend(name: &str) -> AnyResult<u32> {
    match name.to_ascii_lowercase().as_str() {
        "auto" => Ok(DXB_BACKEND_AUTO),
        "dx11" => Ok(DXB_BACKEND_DX11),
        "dx12" => Ok(DXB_BACKEND_DX12),
        _ => Err(format!("unknown backend \"{}\"", name).into()),
    }
}

pub fn result_name(result: DxbResult) -> String {
    match result {
        DXB_OK => "DXB_OK".to_string(),
        DXB_E_INVALID_HANDLE => "DXB_E_INVALID_HANDLE".to_string(),
        DXB_E_DEVICE_LOST => "DXB_E_DEVICE_LOST".to_string(),
        DXB_E_OUT_OF_MEMORY => "DXB_E_OUT_OF_MEMORY".to_string(),
        DXB_E_INVALID_ARG => "DXB_E_INVALID_ARG".to_string(),
        DXB_E_NOT_SUPPORTED => "DXB_E_NOT_SUPPORTED".to_string(),
        DXB_E_SHADER_COMPILE => "DXB_E_SHADER_COMPILE".to_string(),
        DXB_E_NOT_INITIALIZED => "DXB_E_NOT_INITIALIZED".to_string(),
        DXB_E_INVALID_STATE => "DXB_E_INVALID_STATE".to_string(),
        DXB_E_INTERNAL => "DXB_E_INTERNAL".to_string(),
        _ => format!("DXBResult({})", result),
    }
}

pub fn format_bytes(value: u64) -> String {
    let units = ["B", "KiB", "MiB", "GiB", "TiB"];
    let mut amount = value as f64;
    let mut unit = units[0];
    for candidate in units {
        unit = candidate;
        if amount < 1024.0 || candidate == units[units.len() - 1] {
            break;
        }
        amount /= 1024.0;
    }
    if unit == "B" {
        format!("{} {}", value, unit)
    } else {
        format!("{:.1} {}", amount, unit)
    }
}

pub fn format_hresult(value: u32) -> String {
    format!("0x{:08X}", value)
}

pub fn format_feature_level(value: u32) -> String {
    if value == 0 {
        return "n/a".to_string();
    }
    let major = (value >> 12) & 0xF;
    let minor = (value >> 8) & 0xF;
    if major == 0 && minor == 0 {
        return format_hresult(value);
    }
    format!("{}_{}", major, minor)
}

pub fn decode_description(raw: &[u8; 128]) -> String {
    let len = raw.iter().position(|byte| *byte == 0).unwrap_or(raw.len());
    String::from_utf8_lossy(&raw[..len]).into_owned()
}

pub fn versioned_input_element(
    semantic: u32,
    semantic_index: u32,
    format: u32,
    input_slot: u32,
    byte_offset: u32,
) -> DxbInputElementDesc {
    DxbInputElementDesc {
        struct_version: DXBRIDGE_STRUCT_VERSION,
        semantic,
        semantic_index,
        format,
        input_slot,
        byte_offset,
    }
}

fn locate_dll(explicit_path: Option<&str>) -> AnyResult<PathBuf> {
    let mut candidates = Vec::<PathBuf>::new();

    if let Some(path) = explicit_path.filter(|value| !value.trim().is_empty()) {
        candidates.push(PathBuf::from(path));
    }

    if let Ok(path) = std::env::var("DXBRIDGE_DLL") {
        if !path.trim().is_empty() {
            candidates.push(PathBuf::from(path));
        }
    }

    candidates.push(PathBuf::from("dxbridge.dll"));

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(parent) = exe_path.parent() {
            candidates.push(parent.join("dxbridge.dll"));
        }
    }

    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    candidates.push(manifest_dir.join("dxbridge.dll"));

    if let Some(repo_root) = find_repo_root(&manifest_dir) {
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("debug")
                .join("Debug")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("debug")
                .join("examples")
                .join("Debug")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("debug")
                .join("tests")
                .join("Debug")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("release")
                .join("Release")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("release")
                .join("examples")
                .join("Release")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("release")
                .join("tests")
                .join("Release")
                .join("dxbridge.dll"),
        );
        candidates.push(
            repo_root
                .join("out")
                .join("build")
                .join("ci")
                .join("Release")
                .join("dxbridge.dll"),
        );
    }

    let mut searched = Vec::new();
    for candidate in dedupe_paths(candidates) {
        let resolved = if candidate.is_absolute() {
            candidate
        } else {
            std::env::current_dir()?.join(candidate)
        };
        searched.push(resolved.display().to_string());
        if resolved.is_file() {
            return Ok(resolved);
        }
    }

    Err(format!(
        "could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\nSearched:\n  - {}",
        searched.join("\n  - ")
    )
    .into())
}

fn dedupe_paths(paths: Vec<PathBuf>) -> Vec<PathBuf> {
    let mut seen = std::collections::BTreeSet::new();
    let mut unique = Vec::new();
    for path in paths {
        let normalized = path.to_string_lossy().to_ascii_lowercase();
        if seen.insert(normalized) {
            unique.push(path);
        }
    }
    unique
}

fn find_repo_root(start: &Path) -> Option<PathBuf> {
    let mut current = Some(start);
    while let Some(path) = current {
        if path
            .join("include")
            .join("dxbridge")
            .join("dxbridge.h")
            .is_file()
        {
            return Some(path.to_path_buf());
        }
        current = path.parent();
    }
    None
}
