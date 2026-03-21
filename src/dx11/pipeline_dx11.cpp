// src/dx11/pipeline_dx11.cpp
// DX11Backend — input layout and pipeline state.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>
#include <vector>
#include <string>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// CreateInputLayout
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateInputLayout(DXBDevice device,
                                          const DXBInputElementDesc* elems,
                                          uint32_t count,
                                          DXBShader vs_handle,
                                          DXBInputLayout* out)
{
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    if (!elems || count == 0) {
        SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: no elements");
        return DXB_E_INVALID_ARG;
    }

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateInputLayout: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    auto* vs_obj = m_shaders.Get(vs_handle, ObjectType::Shader);
    if (!vs_obj || vs_obj->stage != DXB_SHADER_STAGE_VERTEX) {
        SetLastError(DXB_E_INVALID_HANDLE,
                     "CreateInputLayout: invalid vertex shader handle");
        return DXB_E_INVALID_HANDLE;
    }
    if (vs_obj->bytecode.empty()) {
        SetLastError(DXB_E_INVALID_STATE,
                     "CreateInputLayout: vertex shader has no bytecode");
        return DXB_E_INVALID_STATE;
    }

    // Build D3D11_INPUT_ELEMENT_DESC array
    // We need to keep semantic name strings alive for the D3D11 call.
    // They're pointers into static string literals, so that's fine.
    std::vector<D3D11_INPUT_ELEMENT_DESC> d3d_elems;
    d3d_elems.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const DXBInputElementDesc& e = elems[i];
        D3D11_INPUT_ELEMENT_DESC d = {};
        d.SemanticName         = DXBSemanticToString(e.semantic);
        d.SemanticIndex        = e.semantic_index;
        d.Format               = DXBFormatToDXGI(e.format);
        // GAP 3: reject unknown/unrecognised format values immediately
        if (d.Format == DXGI_FORMAT_UNKNOWN) {
            SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: unrecognised format in element %u", i);
            return DXB_E_INVALID_ARG;
        }
        d.InputSlot            = e.input_slot;
        d.AlignedByteOffset    = (e.byte_offset == 0xFFFFFFFF)
                                     ? D3D11_APPEND_ALIGNED_ELEMENT
                                     : e.byte_offset;
        d.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        d.InstanceDataStepRate = 0;
        d3d_elems.push_back(d);
    }

    ComPtr<ID3D11InputLayout> layout;
    HRESULT hr = dev_obj->device->CreateInputLayout(
        d3d_elems.data(),
        static_cast<UINT>(d3d_elems.size()),
        vs_obj->bytecode.data(),
        vs_obj->bytecode.size(),
        layout.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateInputLayout");
        return MapHRESULT(hr);
    }

    auto* il_obj    = new InputLayoutObjectDX11();
    il_obj->layout  = layout;

    *out = m_input_layouts.Alloc(il_obj, ObjectType::InputLayout);
    return DXB_OK;
}

void DX11Backend::DestroyInputLayout(DXBInputLayout layout) {
    auto* obj = m_input_layouts.Get(layout, ObjectType::InputLayout);
    if (!obj) return;
    m_input_layouts.Free(layout, ObjectType::InputLayout);
    delete obj;
}

// ---------------------------------------------------------------------------
// CreatePipelineState
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreatePipelineState(DXBDevice device,
                                             const DXBPipelineDesc* desc,
                                             DXBPipeline* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreatePipelineState: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreatePipelineState: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    auto* vs_obj = m_shaders.Get(desc->vs, ObjectType::Shader);
    if (!vs_obj || vs_obj->stage != DXB_SHADER_STAGE_VERTEX) {
        SetLastError(DXB_E_INVALID_HANDLE,
                     "CreatePipelineState: invalid vertex shader");
        return DXB_E_INVALID_HANDLE;
    }

    auto* ps_obj = m_shaders.Get(desc->ps, ObjectType::Shader);
    if (!ps_obj || ps_obj->stage != DXB_SHADER_STAGE_PIXEL) {
        SetLastError(DXB_E_INVALID_HANDLE,
                     "CreatePipelineState: invalid pixel shader");
        return DXB_E_INVALID_HANDLE;
    }

    InputLayoutObjectDX11* il_obj = nullptr;
    if (desc->input_layout != DXBRIDGE_NULL_HANDLE) {
        il_obj = m_input_layouts.Get(desc->input_layout, ObjectType::InputLayout);
        if (!il_obj) {
            SetLastError(DXB_E_INVALID_HANDLE,
                         "CreatePipelineState: invalid input layout");
            return DXB_E_INVALID_HANDLE;
        }
    }

    auto* pipe = new PipelineObjectDX11();
    HRESULT hr = S_OK;

    // Rasterizer state
    D3D11_RASTERIZER_DESC rs_desc = {};
    rs_desc.FillMode              = desc->wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
    rs_desc.CullMode              = desc->cull_back ? D3D11_CULL_BACK : D3D11_CULL_NONE;
    rs_desc.FrontCounterClockwise = FALSE;
    rs_desc.DepthBias             = 0;
    rs_desc.DepthBiasClamp        = 0.0f;
    rs_desc.SlopeScaledDepthBias  = 0.0f;
    rs_desc.DepthClipEnable       = TRUE;
    rs_desc.ScissorEnable         = FALSE;
    rs_desc.MultisampleEnable     = FALSE;
    rs_desc.AntialiasedLineEnable = FALSE;
    hr = dev_obj->device->CreateRasterizerState(&rs_desc,
                                                 pipe->rasterizer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateRasterizerState");
        delete pipe;
        return MapHRESULT(hr);
    }

    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC ds_desc = {};
    ds_desc.DepthEnable    = desc->depth_test  ? TRUE : FALSE;
    ds_desc.DepthWriteMask = desc->depth_write ? D3D11_DEPTH_WRITE_MASK_ALL
                                                : D3D11_DEPTH_WRITE_MASK_ZERO;
    ds_desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    ds_desc.StencilEnable  = FALSE;
    ds_desc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
    ds_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    // Front/back stencil ops default (not used)
    ds_desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    ds_desc.BackFace = ds_desc.FrontFace;

    hr = dev_obj->device->CreateDepthStencilState(&ds_desc,
                                                   pipe->depth_stencil.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateDepthStencilState");
        delete pipe;
        return MapHRESULT(hr);
    }

    // Blend state (opaque default)
    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.AlphaToCoverageEnable  = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    blend_desc.RenderTarget[0].BlendEnable           = FALSE;
    blend_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = dev_obj->device->CreateBlendState(&blend_desc,
                                            pipe->blend.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateBlendState");
        delete pipe;
        return MapHRESULT(hr);
    }

    // Store shader references
    pipe->vs = vs_obj->vs;
    pipe->ps = ps_obj->ps;
    if (il_obj) {
        pipe->input_layout = il_obj->layout;
    }
    pipe->topology = DXBTopologyToD3D11(desc->topology);

    *out = m_pipelines.Alloc(pipe, ObjectType::Pipeline);
    return DXB_OK;
}

void DX11Backend::DestroyPipeline(DXBPipeline pipeline) {
    auto* obj = m_pipelines.Get(pipeline, ObjectType::Pipeline);
    if (!obj) return;
    m_pipelines.Free(pipeline, ObjectType::Pipeline);
    delete obj;
}

} // namespace dxb
