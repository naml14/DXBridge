// src/common/backend.hpp
// IBackend pure-virtual interface — all DX11/DX12 backends implement this.
#pragma once
#include "../../include/dxbridge/dxbridge.h"

namespace dxb {

struct IBackend {
    virtual ~IBackend() = default;

    // Lifecycle
    virtual DXBResult Init()     = 0;
    virtual void      Shutdown() = 0;

    // Adapter enumeration
    virtual DXBResult EnumerateAdapters(DXBAdapterInfo* out_buf,
                                         uint32_t* in_out_count) = 0;

    // Feature query
    virtual uint32_t  SupportsFeature(uint32_t feature_flag) = 0;

    // Device
    virtual DXBResult CreateDevice(const DXBDeviceDesc* desc,
                                    DXBDevice* out_device) = 0;
    virtual void      DestroyDevice(DXBDevice device) = 0;
    virtual uint32_t  GetFeatureLevel(DXBDevice device) = 0;

    // Swap chain
    virtual DXBResult CreateSwapChain(DXBDevice device,
                                       const DXBSwapChainDesc* desc,
                                       DXBSwapChain* out_sc) = 0;
    virtual void      DestroySwapChain(DXBSwapChain sc) = 0;
    virtual DXBResult ResizeSwapChain(DXBSwapChain sc,
                                       uint32_t width, uint32_t height) = 0;
    virtual DXBResult GetBackBuffer(DXBSwapChain sc,
                                     DXBRenderTarget* out_rt) = 0;

    // Render target / depth views
    virtual DXBResult CreateRenderTargetView(DXBDevice device,
                                              DXBRenderTarget rt,
                                              DXBRTV* out_rtv) = 0;
    virtual void      DestroyRTV(DXBRTV rtv) = 0;
    virtual DXBResult CreateDepthStencilView(DXBDevice device,
                                              const DXBDepthStencilDesc* desc,
                                              DXBDSV* out_dsv) = 0;
    virtual void      DestroyDSV(DXBDSV dsv) = 0;

    // Shaders
    virtual DXBResult CompileShader(DXBDevice device,
                                     const DXBShaderDesc* desc,
                                     DXBShader* out_shader) = 0;
    virtual void      DestroyShader(DXBShader shader) = 0;

    // Input layout
    virtual DXBResult CreateInputLayout(DXBDevice device,
                                         const DXBInputElementDesc* elements,
                                         uint32_t element_count,
                                         DXBShader vs,
                                         DXBInputLayout* out_layout) = 0;
    virtual void      DestroyInputLayout(DXBInputLayout layout) = 0;

    // Pipeline
    virtual DXBResult CreatePipelineState(DXBDevice device,
                                           const DXBPipelineDesc* desc,
                                           DXBPipeline* out_pipeline) = 0;
    virtual void      DestroyPipeline(DXBPipeline pipeline) = 0;

    // Buffers
    virtual DXBResult CreateBuffer(DXBDevice device,
                                    const DXBBufferDesc* desc,
                                    DXBBuffer* out_buf) = 0;
    virtual DXBResult UploadData(DXBBuffer buf,
                                  const void* data, uint32_t byte_size) = 0;
    virtual void      DestroyBuffer(DXBBuffer buf) = 0;

    // Render commands
    virtual DXBResult BeginFrame(DXBDevice device)                           = 0;
    virtual DXBResult SetRenderTargets(DXBDevice device,
                                        DXBRTV rtv, DXBDSV dsv)             = 0;
    virtual DXBResult ClearRenderTarget(DXBRTV rtv,
                                         const float rgba[4])                = 0;
    virtual DXBResult ClearDepthStencil(DXBDSV dsv,
                                         float depth, uint8_t stencil)       = 0;
    virtual DXBResult SetViewport(DXBDevice device,
                                   const DXBViewport* vp)                    = 0;
    virtual DXBResult SetPipeline(DXBDevice device, DXBPipeline pipeline)    = 0;
    virtual DXBResult SetVertexBuffer(DXBDevice device, DXBBuffer buf,
                                       uint32_t stride, uint32_t offset)     = 0;
    virtual DXBResult SetIndexBuffer(DXBDevice device, DXBBuffer buf,
                                      DXBFormat fmt, uint32_t offset)        = 0;
    virtual DXBResult SetConstantBuffer(DXBDevice device, uint32_t slot,
                                         DXBBuffer buf)                      = 0;
    virtual DXBResult Draw(DXBDevice device, uint32_t vertex_count,
                            uint32_t start_vertex)                           = 0;
    virtual DXBResult DrawIndexed(DXBDevice device, uint32_t index_count,
                                   uint32_t start_index, int32_t base_vertex)= 0;
    virtual DXBResult EndFrame(DXBDevice device)                             = 0;
    virtual DXBResult Present(DXBSwapChain sc, uint32_t sync_interval)       = 0;

    // Native handle escape hatches
    virtual void* GetNativeDevice(DXBDevice device)  = 0;
    virtual void* GetNativeContext(DXBDevice device) = 0;
};

// Global backend pointer — set once at DXBridge_Init, never changes after.
extern IBackend* g_backend;

} // namespace dxb
