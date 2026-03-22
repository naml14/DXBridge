// include/dxbridge/dxbridge.h
// DXBridge -- Direct3D abstraction layer, C ABI
// Minimum Windows SDK: 10.0.22000.0  Minimum OS: Windows 10 1809

#pragma once
#include <stdint.h>
#include "dxbridge_version.h"

#ifdef DXBRIDGE_EXPORTS
  #define DXBRIDGE_API __declspec(dllexport)
#else
  #define DXBRIDGE_API __declspec(dllimport)
#endif

#define DXBRIDGE_NULL_HANDLE     ((uint64_t)0)

// --- Opaque handles ----------------------------------------------------------
typedef uint64_t DXBDevice;
typedef uint64_t DXBSwapChain;
typedef uint64_t DXBBuffer;
typedef uint64_t DXBShader;
typedef uint64_t DXBPipeline;
typedef uint64_t DXBInputLayout;
typedef uint64_t DXBRTV;           // Render Target View
typedef uint64_t DXBDSV;           // Depth Stencil View
typedef uint64_t DXBRenderTarget;

// --- Result codes ------------------------------------------------------------
typedef int32_t DXBResult;
#define DXB_OK                  ((DXBResult) 0)
#define DXB_E_INVALID_HANDLE    ((DXBResult)-1)
#define DXB_E_DEVICE_LOST       ((DXBResult)-2)
#define DXB_E_OUT_OF_MEMORY     ((DXBResult)-3)
#define DXB_E_INVALID_ARG       ((DXBResult)-4)
#define DXB_E_NOT_SUPPORTED     ((DXBResult)-5)
#define DXB_E_SHADER_COMPILE    ((DXBResult)-6)
#define DXB_E_NOT_INITIALIZED   ((DXBResult)-7)
#define DXB_E_INVALID_STATE     ((DXBResult)-8)
#define DXB_E_INTERNAL          ((DXBResult)-999)

// --- Enumerations (all uint32_t) ---------------------------------------------
typedef uint32_t DXBBackend;
#define DXB_BACKEND_AUTO  ((DXBBackend)0)
#define DXB_BACKEND_DX11  ((DXBBackend)1)
#define DXB_BACKEND_DX12  ((DXBBackend)2)

typedef uint32_t DXBFormat;
#define DXB_FORMAT_UNKNOWN         ((DXBFormat)0)
#define DXB_FORMAT_RGBA8_UNORM     ((DXBFormat)1)
#define DXB_FORMAT_RGBA16_FLOAT    ((DXBFormat)2)
#define DXB_FORMAT_D32_FLOAT       ((DXBFormat)3)
#define DXB_FORMAT_D24_UNORM_S8    ((DXBFormat)4)
#define DXB_FORMAT_R32_UINT        ((DXBFormat)5)   // index buffer
#define DXB_FORMAT_RG32_FLOAT      ((DXBFormat)6)   /* DXGI_FORMAT_R32G32_FLOAT        */
#define DXB_FORMAT_RGB32_FLOAT     ((DXBFormat)7)   /* DXGI_FORMAT_R32G32B32_FLOAT      */
#define DXB_FORMAT_RGBA32_FLOAT    ((DXBFormat)8)   /* DXGI_FORMAT_R32G32B32A32_FLOAT   */

typedef uint32_t DXBDeviceFlags;
#define DXB_DEVICE_FLAG_DEBUG           ((DXBDeviceFlags)0x0001)
#define DXB_DEVICE_FLAG_GPU_VALIDATION  ((DXBDeviceFlags)0x0002)

typedef uint32_t DXBBufferFlags;
#define DXB_BUFFER_FLAG_VERTEX   ((DXBBufferFlags)0x0001)
#define DXB_BUFFER_FLAG_INDEX    ((DXBBufferFlags)0x0002)
#define DXB_BUFFER_FLAG_CONSTANT ((DXBBufferFlags)0x0004)
#define DXB_BUFFER_FLAG_DYNAMIC  ((DXBBufferFlags)0x0008)  // CPU writable every frame

typedef uint32_t DXBShaderStage;
#define DXB_SHADER_STAGE_VERTEX  ((DXBShaderStage)0)
#define DXB_SHADER_STAGE_PIXEL   ((DXBShaderStage)1)

typedef uint32_t DXBPrimTopology;
#define DXB_PRIM_TRIANGLELIST  ((DXBPrimTopology)0)
#define DXB_PRIM_TRIANGLESTRIP ((DXBPrimTopology)1)
#define DXB_PRIM_LINELIST      ((DXBPrimTopology)2)
#define DXB_PRIM_POINTLIST     ((DXBPrimTopology)3)

typedef uint32_t DXBInputSemantic;
#define DXB_SEMANTIC_POSITION ((DXBInputSemantic)0)
#define DXB_SEMANTIC_NORMAL   ((DXBInputSemantic)1)
#define DXB_SEMANTIC_TEXCOORD ((DXBInputSemantic)2)
#define DXB_SEMANTIC_COLOR    ((DXBInputSemantic)3)

typedef uint32_t DXBFeatureFlag;
#define DXB_FEATURE_DX12      ((DXBFeatureFlag)0x0001)

typedef uint32_t DXBCapability;
#define DXB_CAPABILITY_BACKEND_AVAILABLE      ((DXBCapability)1)
#define DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE  ((DXBCapability)2)
#define DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE ((DXBCapability)3)
#define DXB_CAPABILITY_ADAPTER_COUNT          ((DXBCapability)4)
#define DXB_CAPABILITY_ADAPTER_SOFTWARE       ((DXBCapability)5)
#define DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL ((DXBCapability)6)

// --- Structs (struct_version at offset 0, POD only, fixed-width types) -------

typedef struct DXBAdapterInfo {
    uint32_t struct_version;        // = DXBRIDGE_STRUCT_VERSION
    uint32_t index;
    char     description[128];
    uint64_t dedicated_video_mem;
    uint64_t dedicated_system_mem;
    uint64_t shared_system_mem;
    uint32_t is_software;           // 1 = WARP
} DXBAdapterInfo;

typedef struct DXBDeviceDesc {
    uint32_t     struct_version;    // = DXBRIDGE_STRUCT_VERSION
    DXBBackend   backend;
    uint32_t     adapter_index;     // 0 = default (high-perf)
    DXBDeviceFlags flags;
} DXBDeviceDesc;

typedef struct DXBSwapChainDesc {
    uint32_t  struct_version;
    void*     hwnd;                 // HWND -- caller provides window handle
    uint32_t  width;
    uint32_t  height;
    DXBFormat format;               // DXB_FORMAT_RGBA8_UNORM typical
    uint32_t  buffer_count;         // 2 = double-buffer
    uint32_t  flags;                // reserved, set to 0
} DXBSwapChainDesc;

typedef struct DXBBufferDesc {
    uint32_t      struct_version;
    DXBBufferFlags flags;
    uint32_t      byte_size;
    uint32_t      stride;           // vertex stride (bytes), 0 for index/constant
} DXBBufferDesc;

typedef struct DXBShaderDesc {
    uint32_t      struct_version;
    DXBShaderStage stage;
    const char*   source_code;      // HLSL source (UTF-8)
    uint32_t      source_size;      // bytes, 0 = strlen(source_code)
    const char*   source_name;      // for error messages (file path or label)
    const char*   entry_point;      // e.g. "VSMain"
    const char*   target;           // e.g. "vs_5_0"
    uint32_t      compile_flags;    // 0 = defaults; DXB_COMPILE_DEBUG = 0x1
} DXBShaderDesc;

typedef struct DXBInputElementDesc {
    uint32_t       struct_version;
    DXBInputSemantic semantic;
    uint32_t       semantic_index;  // e.g. TEXCOORD0 -> index=0
    DXBFormat      format;
    uint32_t       input_slot;      // vertex buffer slot (0 for single VB)
    uint32_t       byte_offset;     // D3D11_APPEND_ALIGNED_ELEMENT = 0xFFFFFFFF
} DXBInputElementDesc;

typedef struct DXBDepthStencilDesc {
    uint32_t  struct_version;
    DXBFormat format;               // DXB_FORMAT_D32_FLOAT or D24_UNORM_S8
    uint32_t  width;
    uint32_t  height;
} DXBDepthStencilDesc;

typedef struct DXBPipelineDesc {
    uint32_t        struct_version;
    DXBShader       vs;
    DXBShader       ps;
    DXBInputLayout  input_layout;
    DXBPrimTopology topology;
    uint32_t        depth_test;     // 1 = enable
    uint32_t        depth_write;    // 1 = write
    uint32_t        cull_back;      // 1 = cull back face, 0 = none
    uint32_t        wireframe;      // 1 = wireframe
} DXBPipelineDesc;

typedef struct DXBViewport {
    float x, y, width, height;
    float min_depth;    // typically 0.0f
    float max_depth;    // typically 1.0f
} DXBViewport;

typedef struct DXBCapabilityQuery {
    uint32_t      struct_version;
    DXBCapability capability;
    DXBBackend    backend;          // explicit backend target; AUTO is invalid for current queries
    uint32_t      adapter_index;    // used by adapter capabilities
    DXBFormat     format;           // reserved for future format capabilities
    DXBDevice     device;           // reserved for future device-scoped capabilities
    uint32_t      reserved[4];
} DXBCapabilityQuery;

typedef struct DXBCapabilityInfo {
    uint32_t      struct_version;
    DXBCapability capability;
    DXBBackend    backend;
    uint32_t      adapter_index;
    uint32_t      supported;        // 1 when the query resolved for the requested target
    uint32_t      value_u32;        // capability-specific numeric payload
    uint64_t      value_u64;        // reserved for future larger payloads
    uint32_t      reserved[4];
} DXBCapabilityInfo;

// static_asserts (C++ only) verify struct_version is at offset 0
#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(DXBAdapterInfo,    struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBDeviceDesc,     struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBSwapChainDesc,  struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBBufferDesc,     struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBShaderDesc,     struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBInputElementDesc,struct_version)== 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBDepthStencilDesc,struct_version)== 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBPipelineDesc,   struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBCapabilityQuery,struct_version) == 0, "struct_version must be at offset 0");
static_assert(offsetof(DXBCapabilityInfo, struct_version) == 0, "struct_version must be at offset 0");

static_assert(sizeof(DXBDevice)       == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBSwapChain)    == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBBuffer)       == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBShader)       == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBPipeline)     == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBInputLayout)  == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBRTV)          == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBDSV)          == 8, "handle must be 8 bytes");
static_assert(sizeof(DXBRenderTarget) == 8, "handle must be 8 bytes");
#endif

// --- API Functions -----------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Logging
typedef void (*DXBLogCallback)(int level, const char* msg, void* user_data);
DXBRIDGE_API void DXBridge_SetLogCallback(DXBLogCallback cb, void* user_data);

// Init / shutdown
DXBRIDGE_API DXBResult DXBridge_Init(DXBBackend backend_hint);
DXBRIDGE_API void      DXBridge_Shutdown(void);
DXBRIDGE_API uint32_t  DXBridge_GetVersion(void);
DXBRIDGE_API uint32_t  DXBridge_SupportsFeature(uint32_t feature_flag);
DXBRIDGE_API DXBResult DXBridge_QueryCapability(const DXBCapabilityQuery* query,
                                                DXBCapabilityInfo* out_info);

// Error info
DXBRIDGE_API void      DXBridge_GetLastError(char* buf, int buf_size);
DXBRIDGE_API uint32_t  DXBridge_GetLastHRESULT(void);

// Adapters
DXBRIDGE_API DXBResult DXBridge_EnumerateAdapters(DXBAdapterInfo* out_buf,
                                                    uint32_t* in_out_count);

// Device
DXBRIDGE_API DXBResult DXBridge_CreateDevice(const DXBDeviceDesc* desc,
                                               DXBDevice* out_device);
DXBRIDGE_API void      DXBridge_DestroyDevice(DXBDevice device);
DXBRIDGE_API uint32_t  DXBridge_GetFeatureLevel(DXBDevice device);

// Swap chain
DXBRIDGE_API DXBResult DXBridge_CreateSwapChain(DXBDevice device,
                                                  const DXBSwapChainDesc* desc,
                                                  DXBSwapChain* out_sc);
DXBRIDGE_API void      DXBridge_DestroySwapChain(DXBSwapChain sc);
DXBRIDGE_API DXBResult DXBridge_ResizeSwapChain(DXBSwapChain sc,
                                                  uint32_t width, uint32_t height);
DXBRIDGE_API DXBResult DXBridge_GetBackBuffer(DXBSwapChain sc,
                                               DXBRenderTarget* out_rt);

// Render target & depth views
DXBRIDGE_API DXBResult DXBridge_CreateRenderTargetView(DXBDevice device,
                                                         DXBRenderTarget rt,
                                                         DXBRTV* out_rtv);
DXBRIDGE_API void      DXBridge_DestroyRTV(DXBRTV rtv);
DXBRIDGE_API DXBResult DXBridge_CreateDepthStencilView(DXBDevice device,
                                                         const DXBDepthStencilDesc* desc,
                                                         DXBDSV* out_dsv);
DXBRIDGE_API void      DXBridge_DestroyDSV(DXBDSV dsv);

// Shaders
DXBRIDGE_API DXBResult DXBridge_CompileShader(DXBDevice device,
                                               const DXBShaderDesc* desc,
                                               DXBShader* out_shader);
DXBRIDGE_API void      DXBridge_DestroyShader(DXBShader shader);

// Input layout
DXBRIDGE_API DXBResult DXBridge_CreateInputLayout(DXBDevice device,
                                                    const DXBInputElementDesc* elements,
                                                    uint32_t element_count,
                                                    DXBShader vs,
                                                    DXBInputLayout* out_layout);
DXBRIDGE_API void      DXBridge_DestroyInputLayout(DXBInputLayout layout);

// Pipeline
DXBRIDGE_API DXBResult DXBridge_CreatePipelineState(DXBDevice device,
                                                      const DXBPipelineDesc* desc,
                                                      DXBPipeline* out_pipeline);
DXBRIDGE_API void      DXBridge_DestroyPipeline(DXBPipeline pipeline);

// Buffers
DXBRIDGE_API DXBResult DXBridge_CreateBuffer(DXBDevice device,
                                               const DXBBufferDesc* desc,
                                               DXBBuffer* out_buf);
DXBRIDGE_API DXBResult DXBridge_UploadData(DXBBuffer buf,
                                             const void* data, uint32_t byte_size);
DXBRIDGE_API void      DXBridge_DestroyBuffer(DXBBuffer buf);

// Render commands
DXBRIDGE_API DXBResult DXBridge_BeginFrame(DXBDevice device);
DXBRIDGE_API DXBResult DXBridge_SetRenderTargets(DXBDevice device,
                                                   DXBRTV rtv, DXBDSV dsv);
DXBRIDGE_API DXBResult DXBridge_ClearRenderTarget(DXBRTV rtv,
                                                    const float rgba[4]);
DXBRIDGE_API DXBResult DXBridge_ClearDepthStencil(DXBDSV dsv,
                                                    float depth, uint8_t stencil);
DXBRIDGE_API DXBResult DXBridge_SetViewport(DXBDevice device,
                                              const DXBViewport* vp);
DXBRIDGE_API DXBResult DXBridge_SetPipeline(DXBDevice device,
                                              DXBPipeline pipeline);
DXBRIDGE_API DXBResult DXBridge_SetVertexBuffer(DXBDevice device,
                                                  DXBBuffer buf,
                                                  uint32_t stride,
                                                  uint32_t offset);
DXBRIDGE_API DXBResult DXBridge_SetIndexBuffer(DXBDevice device,
                                                 DXBBuffer buf,
                                                 DXBFormat fmt,
                                                 uint32_t offset);
DXBRIDGE_API DXBResult DXBridge_SetConstantBuffer(DXBDevice device,
                                                    uint32_t slot,
                                                    DXBBuffer buf);
DXBRIDGE_API DXBResult DXBridge_Draw(DXBDevice device,
                                       uint32_t vertex_count,
                                       uint32_t start_vertex);
DXBRIDGE_API DXBResult DXBridge_DrawIndexed(DXBDevice device,
                                              uint32_t index_count,
                                              uint32_t start_index,
                                              int32_t  base_vertex);
DXBRIDGE_API DXBResult DXBridge_EndFrame(DXBDevice device);
DXBRIDGE_API DXBResult DXBridge_Present(DXBSwapChain sc,
                                          uint32_t sync_interval);

// Escape hatches
DXBRIDGE_API void*     DXBridge_GetNativeDevice(DXBDevice device);
DXBRIDGE_API void*     DXBridge_GetNativeContext(DXBDevice device);

#ifdef __cplusplus
} // extern "C"
#endif
