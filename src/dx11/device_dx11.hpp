// src/dx11/device_dx11.hpp
// DX11Backend — full Phase 3 implementation.
#pragma once
#include "../common/backend.hpp"
#include "../common/handle_table.hpp"
#include "../common/format_util.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace dxb {

// ---------------------------------------------------------------------------
// GPU object structs stored in handle pools
// ---------------------------------------------------------------------------

struct DeviceObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11Device>        device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIAdapter>        adapter;
    D3D_FEATURE_LEVEL                           feature_level = D3D_FEATURE_LEVEL_11_0;
    bool                                        debug_enabled = false;
};

struct SwapChainObjectDX11 {
    Microsoft::WRL::ComPtr<IDXGISwapChain1>  swapchain;
    Microsoft::WRL::ComPtr<ID3D11Device>     device_ref;   // non-owning (borrowed)
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_ref;
    uint32_t  width  = 0;
    uint32_t  height = 0;
    DXBFormat format = DXB_FORMAT_RGBA8_UNORM;
};

struct RenderTargetObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    bool from_backbuffer = false; // re-acquired on resize
};

struct RTVObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    // keep context reference so Clear can call ClearRenderTargetView
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    context_ref;
};

struct DSVObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>        texture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    context_ref;
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct BufferObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11Buffer>            buffer;
    Microsoft::WRL::ComPtr<ID3D11Device>            device_ref;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     context_ref;
    DXBBufferFlags flags     = 0;
    uint32_t       byte_size = 0;
};

struct ShaderObjectDX11 {
    DXBShaderStage stage = DXB_SHADER_STAGE_VERTEX;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
    std::vector<uint8_t> bytecode; // kept alive for CreateInputLayout
};

struct InputLayoutObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
};

struct PipelineObjectDX11 {
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       input_layout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       ps;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   rasterizer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        blend;
    D3D11_PRIMITIVE_TOPOLOGY                        topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

// ---------------------------------------------------------------------------
// DX11Backend
// ---------------------------------------------------------------------------

class DX11Backend : public IBackend {
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

    // Log callback (stored here; passed to backend log if needed)
    void SetLogCallback(DXBLogCallback cb, void* ud);

private:
    // DXGI factory (created at Init)
    Microsoft::WRL::ComPtr<IDXGIFactory2> m_factory;
    bool                                  m_initialized = false;

    // Handle pools — one per object type
    HandlePool<DeviceObjectDX11>      m_devices;
    HandlePool<SwapChainObjectDX11>   m_swapchains;
    HandlePool<RenderTargetObjectDX11>m_render_targets;
    HandlePool<RTVObjectDX11>         m_rtvs;
    HandlePool<DSVObjectDX11>         m_dsvs;
    HandlePool<BufferObjectDX11>      m_buffers;
    HandlePool<ShaderObjectDX11>      m_shaders;
    HandlePool<InputLayoutObjectDX11> m_input_layouts;
    HandlePool<PipelineObjectDX11>    m_pipelines;

    // Friend access for helper files that need the pools
    friend class DX11SwapChainHelper;
};

// ---------------------------------------------------------------------------
// DX11-specific topology helper (D3D11_PRIMITIVE_TOPOLOGY is a DX11-only type)
// DXBFormatToDXGI, DXBFormatToTypeless, DXBSemanticToString are in format_util.hpp
// ---------------------------------------------------------------------------
inline D3D11_PRIMITIVE_TOPOLOGY DXBTopologyToD3D11(DXBPrimTopology t) noexcept {
    switch (t) {
        case DXB_PRIM_TRIANGLELIST:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case DXB_PRIM_TRIANGLESTRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case DXB_PRIM_LINELIST:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case DXB_PRIM_POINTLIST:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                     return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

} // namespace dxb
