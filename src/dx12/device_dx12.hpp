// src/dx12/device_dx12.hpp
// DX12Backend — full implementation class and all internal object structs.
#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <mutex>
#include "../common/backend.hpp"
#include "../common/handle_table.hpp"
#include "../common/format_util.hpp"

namespace dxb {

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// DescriptorHeapDX12 — simple bump allocator for CPU-only RTV/DSV heaps
// ---------------------------------------------------------------------------

struct DescriptorHeapDX12 {
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  cpu_start   = {};
    D3D12_GPU_DESCRIPTOR_HANDLE  gpu_start   = {};  // valid only for shader-visible
    UINT                         increment   = 0;   // from GetDescriptorHandleIncrementSize — NEVER hardcode
    uint32_t                     capacity    = 0;
    uint32_t                     count       = 0;   // bump allocator

    // Call once after heap is created.
    void Init(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE type) {
        cpu_start = heap->GetCPUDescriptorHandleForHeapStart();
        gpu_start = heap->GetGPUDescriptorHandleForHeapStart(); // valid only if shader-visible
        increment = dev->GetDescriptorHandleIncrementSize(type); // NEVER hardcode
    }

    bool HasCapacity(uint32_t descriptor_count = 1) const noexcept {
        return descriptor_count <= capacity && count <= capacity - descriptor_count;
    }

    bool TryAllocCPU(D3D12_CPU_DESCRIPTOR_HANDLE* out) noexcept {
        if (!out || !HasCapacity(1)) {
            return false;
        }

        *out = cpu_start;
        out->ptr += static_cast<SIZE_T>(increment) * count;
        ++count;
        return true;
    }

    bool TryAllocCPUBlock(uint32_t descriptor_count,
                          D3D12_CPU_DESCRIPTOR_HANDLE* out_handles) noexcept {
        if (!out_handles || descriptor_count == 0 || !HasCapacity(descriptor_count)) {
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE h = cpu_start;
        h.ptr += static_cast<SIZE_T>(increment) * count;
        for (uint32_t i = 0; i < descriptor_count; ++i) {
            out_handles[i] = h;
            h.ptr += increment;
        }
        count += descriptor_count;
        return true;
    }
};

// ---------------------------------------------------------------------------
// GPU object structs stored in handle pools
// ---------------------------------------------------------------------------

struct DeviceObjectDX12 {
    ComPtr<ID3D12Device>              device;
    ComPtr<IDXGIAdapter>              adapter;
    ComPtr<ID3D12CommandQueue>        cmd_queue;
    ComPtr<ID3D12CommandAllocator>    cmd_allocators[2];    // frame 0, frame 1
    ComPtr<ID3D12GraphicsCommandList> cmd_list;
    ComPtr<ID3D12Fence>               fence;
    HANDLE                            fence_event     = nullptr;
    UINT64                            fence_values[2] = {0, 0};
    uint32_t                          frame_index     = 0;  // current back-buffer index
    D3D_FEATURE_LEVEL                 feature_level   = D3D_FEATURE_LEVEL_12_0;
    bool                              debug_enabled   = false;
    bool                              frame_in_progress = false; // set by BeginFrame, cleared by EndFrame
    bool                              device_lost       = false; // sticky: once true, all GPU calls return DXB_E_DEVICE_LOST
    ComPtr<ID3D12RootSignature>       empty_root_sig;
    DescriptorHeapDX12                rtv_heap;              // CPU-only, 16 slots
    DescriptorHeapDX12                dsv_heap;              // CPU-only, 8 slots
    DXGI_FORMAT                       current_rt_format = DXGI_FORMAT_UNKNOWN;
    DXBSwapChain                      active_sc         = 0; // current swap chain handle
};

struct SwapChainObjectDX12 {
    ComPtr<IDXGISwapChain3>     swapchain;              // QI from IDXGISwapChain1
    ComPtr<ID3D12Resource>      back_buffers[3];        // up to 3 back buffers
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[3]  = {};  // CPU handles in device's rtv_heap
    uint32_t                    buffer_count    = 2;
    uint32_t                    width           = 0;
    uint32_t                    height          = 0;
    DXBFormat                   format          = DXB_FORMAT_RGBA8_UNORM;
    DXBDevice                   device_handle   = 0;   // needed for barrier / resize
};

struct RenderTargetObjectDX12 {
    ID3D12Resource* resource     = nullptr;  // non-owning — owned by SwapChainObjectDX12
    uint32_t        buffer_index = 0;        // index within swap chain back_buffers[]
    DXBSwapChain    sc_handle    = 0;        // parent swap chain
};

struct RTVObjectDX12 {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle    = {};      // slot in device's rtv_heap
    ID3D12Resource*             resource      = nullptr; // non-owning
    DXBDevice                   device_handle = 0;       // needed to find cmd_list for Clear
};

struct DSVObjectDX12 {
    ComPtr<ID3D12Resource>      resource;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle    = {};
    uint32_t                    width         = 0;
    uint32_t                    height        = 0;
    DXBDevice                   device_handle = 0;
};

struct BufferObjectDX12 {
    ComPtr<ID3D12Resource> resource;
    void*                  mapped_ptr    = nullptr;  // persistent Map — never Unmap
    uint32_t               byte_size     = 0;
    DXBBufferFlags         flags         = 0;
    uint32_t               stride        = 0;        // vertex stride (for VBV construction)
    DXBDevice              device_handle = 0;
};

struct ShaderObjectDX12 {
    DXBShaderStage       stage = DXB_SHADER_STAGE_VERTEX;
    std::vector<uint8_t> bytecode;  // DXBC blob copied out of ID3DBlob
};

struct InputLayoutObjectDX12 {
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    std::vector<std::string>              semantic_names;  // owns SemanticName strings
    // elements[i].SemanticName points into semantic_names[i].c_str() — stable after construction
};

struct PipelineObjectDX12 {
    ComPtr<ID3D12PipelineState> pso;
    ID3D12RootSignature*        root_sig  = nullptr;  // borrowed ref from DeviceObjectDX12
    D3D_PRIMITIVE_TOPOLOGY      topology  = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // for IASetPrimitiveTopology
};

// ---------------------------------------------------------------------------
// DX12Backend
// ---------------------------------------------------------------------------

class DX12Backend : public IBackend {
public:
    // Lifecycle
    DXBResult Init()     override;
    void      Shutdown() override;

    // Adapter / feature
    DXBResult EnumerateAdapters(DXBAdapterInfo* buf, uint32_t* count) override;
    uint32_t  SupportsFeature(uint32_t flag)                          override;

    // Device
    DXBResult CreateDevice(const DXBDeviceDesc* desc, DXBDevice* out) override;
    void      DestroyDevice(DXBDevice device)                         override;
    uint32_t  GetFeatureLevel(DXBDevice device)                       override;

    // Swap chain
    DXBResult CreateSwapChain(DXBDevice device,
                               const DXBSwapChainDesc* desc,
                               DXBSwapChain* out)                     override;
    void      DestroySwapChain(DXBSwapChain sc)                       override;
    DXBResult ResizeSwapChain(DXBSwapChain sc,
                               uint32_t w, uint32_t h)                override;
    DXBResult GetBackBuffer(DXBSwapChain sc, DXBRenderTarget* out)    override;

    // Views
    DXBResult CreateRenderTargetView(DXBDevice dev,
                                      DXBRenderTarget rt,
                                      DXBRTV* out)                    override;
    void      DestroyRTV(DXBRTV rtv)                                  override;
    DXBResult CreateDepthStencilView(DXBDevice dev,
                                      const DXBDepthStencilDesc* desc,
                                      DXBDSV* out)                    override;
    void      DestroyDSV(DXBDSV dsv)                                  override;

    // Buffers
    DXBResult CreateBuffer(DXBDevice dev,
                            const DXBBufferDesc* desc,
                            DXBBuffer* out)                           override;
    DXBResult UploadData(DXBBuffer buf,
                          const void* data, uint32_t size)            override;
    void      DestroyBuffer(DXBBuffer buf)                            override;

    // Shaders
    DXBResult CompileShader(DXBDevice dev,
                             const DXBShaderDesc* desc,
                             DXBShader* out)                          override;
    void      DestroyShader(DXBShader shader)                         override;

    // Input layout + pipeline
    DXBResult CreateInputLayout(DXBDevice dev,
                                 const DXBInputElementDesc* elems,
                                 uint32_t count,
                                 DXBShader vs,
                                 DXBInputLayout* out)                 override;
    void      DestroyInputLayout(DXBInputLayout layout)               override;
    DXBResult CreatePipelineState(DXBDevice dev,
                                   const DXBPipelineDesc* desc,
                                   DXBPipeline* out)                  override;
    void      DestroyPipeline(DXBPipeline pipeline)                   override;

    // Render commands
    DXBResult BeginFrame(DXBDevice dev)                               override;
    DXBResult SetRenderTargets(DXBDevice dev,
                                DXBRTV rtv, DXBDSV dsv)              override;
    DXBResult ClearRenderTarget(DXBRTV rtv,
                                 const float rgba[4])                 override;
    DXBResult ClearDepthStencil(DXBDSV dsv,
                                 float depth, uint8_t stencil)       override;
    DXBResult SetViewport(DXBDevice dev,
                           const DXBViewport* vp)                    override;
    DXBResult SetPipeline(DXBDevice dev, DXBPipeline pipeline)       override;
    DXBResult SetVertexBuffer(DXBDevice dev, DXBBuffer buf,
                               uint32_t stride, uint32_t offset)     override;
    DXBResult SetIndexBuffer(DXBDevice dev, DXBBuffer buf,
                              DXBFormat fmt, uint32_t offset)        override;
    DXBResult SetConstantBuffer(DXBDevice dev, uint32_t slot,
                                 DXBBuffer buf)                      override;
    DXBResult Draw(DXBDevice dev,
                   uint32_t count, uint32_t start)                   override;
    DXBResult DrawIndexed(DXBDevice dev, uint32_t count,
                           uint32_t start, int32_t base)             override;
    DXBResult EndFrame(DXBDevice dev)                                 override;
    DXBResult Present(DXBSwapChain sc, uint32_t sync)                override;

    // Escape hatches
    void* GetNativeDevice(DXBDevice dev)                              override;
    void* GetNativeContext(DXBDevice dev)                             override;

    // Log callback
    void SetLogCallback(DXBLogCallback cb, void* ud);

    // Internal test hook for validation coverage.
    DeviceObjectDX12* DebugGetDeviceObject(DXBDevice dev) noexcept {
        return m_devices.Get(dev, ObjectType::Device);
    }

    SwapChainObjectDX12* DebugGetSwapChainObject(DXBSwapChain sc) noexcept {
        return m_swap_chains.Get(sc, ObjectType::SwapChain);
    }

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4>  m_factory;
    bool                                   m_initialized = false;

    HandlePool<DeviceObjectDX12>           m_devices;
    HandlePool<SwapChainObjectDX12>        m_swap_chains;
    HandlePool<BufferObjectDX12>           m_buffers;
    HandlePool<ShaderObjectDX12>           m_shaders;
    HandlePool<InputLayoutObjectDX12>      m_input_layouts;
    HandlePool<PipelineObjectDX12>         m_pipelines;
    HandlePool<RTVObjectDX12>              m_rtvs;
    HandlePool<DSVObjectDX12>              m_dsvs;
    HandlePool<RenderTargetObjectDX12>     m_render_targets;

    DXBLogCallback                         m_log_cb   = nullptr;
    void*                                  m_log_data = nullptr;
};

} // namespace dxb
