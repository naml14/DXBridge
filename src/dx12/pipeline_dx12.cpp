// src/dx12/pipeline_dx12.cpp
// DX12Backend — input layout and pipeline state object (PSO) creation.
// Phase C.3: CreateInputLayout, DestroyInputLayout, CreatePipelineState, DestroyPipeline.
#include "device_dx12.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

namespace {

static bool IsDeviceLostHRESULT(HRESULT hr) noexcept {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

static DXBResult GuardDeviceLost(dxb::DeviceObjectDX12* dev, const char* context) noexcept {
    if (!dev || !dev->device_lost) {
        return DXB_OK;
    }

    dxb::SetLastError(DXB_E_DEVICE_LOST, "%s: device lost", context);
    return DXB_E_DEVICE_LOST;
}

static DXBResult ReturnHRESULT(dxb::DeviceObjectDX12* dev,
                               HRESULT hr,
                               const char* context) noexcept {
    if (IsDeviceLostHRESULT(hr) && dev) {
        dev->device_lost = true;
        dxb::SetLastErrorFromHR(hr, context);
        return DXB_E_DEVICE_LOST;
    }

    dxb::SetLastErrorFromHR(hr, context);
    return dxb::MapHRESULT(hr);
}

} // namespace

namespace dxb {

// ---------------------------------------------------------------------------
// CreateInputLayout — owns semantic name strings (SemanticName raw ptr stability)
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateInputLayout(DXBDevice device_handle,
                                          const DXBInputElementDesc* elements,
                                          uint32_t element_count,
                                          DXBShader /*vs_unused*/,
                                          DXBInputLayout* out)
{
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateInputLayout: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreateInputLayout");
    if (lost != DXB_OK) {
        return lost;
    }

    if (!elements || element_count == 0) {
        SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: no elements");
        return DXB_E_INVALID_ARG;
    }

    auto* layout = new InputLayoutObjectDX12();
    layout->semantic_names.reserve(element_count);
    layout->elements.reserve(element_count);

    for (uint32_t i = 0; i < element_count; ++i) {
        // Store semantic name string — elements[i].SemanticName points here
        layout->semantic_names.push_back(DXBSemanticToString(elements[i].semantic));

        D3D12_INPUT_ELEMENT_DESC e = {};
        // SemanticName MUST be a stable pointer — point into our owned std::string
        e.SemanticName         = layout->semantic_names.back().c_str();
        e.SemanticIndex        = elements[i].semantic_index;
        e.Format               = DXBFormatToDXGI(elements[i].format);
        // GAP 3: reject unknown/unrecognised format values immediately
        if (e.Format == DXGI_FORMAT_UNKNOWN) {
            delete layout;
            SetLastError(DXB_E_INVALID_ARG, "CreateInputLayout: unrecognised format in element %u", i);
            return DXB_E_INVALID_ARG;
        }
        e.InputSlot            = elements[i].input_slot;
        // 0xFFFFFFFF = D3D12_APPEND_ALIGNED_ELEMENT convention
        e.AlignedByteOffset    = (elements[i].byte_offset == 0xFFFFFFFF)
                                   ? D3D12_APPEND_ALIGNED_ELEMENT
                                   : elements[i].byte_offset;
        e.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        e.InstanceDataStepRate = 0;
        layout->elements.push_back(e);
    }

    // IMPORTANT: after all push_back calls the vector may have reallocated.
    // Re-patch SemanticName pointers now that the vector is stable.
    for (uint32_t i = 0; i < element_count; ++i) {
        layout->elements[i].SemanticName = layout->semantic_names[i].c_str();
    }

    *out = m_input_layouts.Alloc(layout, ObjectType::InputLayout);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyInputLayout
// ---------------------------------------------------------------------------

void DX12Backend::DestroyInputLayout(DXBInputLayout layout) {
    auto* obj = m_input_layouts.Get(layout, ObjectType::InputLayout);
    if (!obj) return;
    m_input_layouts.Free(layout, ObjectType::InputLayout);
    delete obj;
}

// ---------------------------------------------------------------------------
// CreatePipelineState — D3D12_GRAPHICS_PIPELINE_STATE_DESC
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreatePipelineState(DXBDevice device_handle,
                                            const DXBPipelineDesc* desc,
                                            DXBPipeline* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreatePipelineState: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreatePipelineState: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreatePipelineState");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* vs_obj = m_shaders.Get(desc->vs, ObjectType::Shader);
    if (!vs_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreatePipelineState: invalid VS handle");
        return DXB_E_INVALID_HANDLE;
    }

    auto* ps_obj = m_shaders.Get(desc->ps, ObjectType::Shader);
    if (!ps_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreatePipelineState: invalid PS handle");
        return DXB_E_INVALID_HANDLE;
    }

    // Input layout is optional (e.g. fullscreen triangle with no VB)
    InputLayoutObjectDX12* il_obj = nullptr;
    if (desc->input_layout != DXBRIDGE_NULL_HANDLE) {
        il_obj = m_input_layouts.Get(desc->input_layout, ObjectType::InputLayout);
        if (!il_obj) {
            SetLastError(DXB_E_INVALID_HANDLE, "CreatePipelineState: invalid InputLayout handle");
            return DXB_E_INVALID_HANDLE;
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = dev->empty_root_sig.Get();

    // Shader bytecode — pointer into ShaderObjectDX12::bytecode (valid for object lifetime)
    pso_desc.VS = { vs_obj->bytecode.data(), vs_obj->bytecode.size() };
    pso_desc.PS = { ps_obj->bytecode.data(), ps_obj->bytecode.size() };

    // Input layout
    if (il_obj) {
        pso_desc.InputLayout = {
            il_obj->elements.data(),
            static_cast<UINT>(il_obj->elements.size())
        };
    }

    // Topology type for PSO (TRIANGLE/LINE/POINT — coarse enum)
    // GOTCHA: Different from D3D_PRIMITIVE_TOPOLOGY used in IASetPrimitiveTopology
    pso_desc.PrimitiveTopologyType = DXBTopologyToD3D12Type(desc->topology);

    // Render targets
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0]    = dev->current_rt_format; // must match the active render target
    pso_desc.DSVFormat        = desc->depth_test ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
    pso_desc.SampleDesc       = { 1, 0 };
    pso_desc.SampleMask       = UINT_MAX;

    if (dev->debug_enabled && dev->current_rt_format == DXGI_FORMAT_UNKNOWN) {
        DXB_LOG_WARN("DX12 CreatePipelineState rejected by debug validation: no active render-target format is known yet");
        SetLastError(DXB_E_INVALID_STATE,
                     "CreatePipelineState: debug validation requires an active render-target format");
        return DXB_E_INVALID_STATE;
    }

    // Rasterizer state — manual initialization (avoid d3dx12.h dependency)
    D3D12_RASTERIZER_DESC& rs  = pso_desc.RasterizerState;
    rs.FillMode              = desc->wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    rs.CullMode              = desc->cull_back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    rs.FrontCounterClockwise = FALSE;
    rs.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
    rs.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rs.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rs.DepthClipEnable       = TRUE;
    rs.MultisampleEnable     = FALSE;
    rs.AntialiasedLineEnable = FALSE;
    rs.ForcedSampleCount     = 0;
    rs.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Depth stencil state
    D3D12_DEPTH_STENCIL_DESC& ds = pso_desc.DepthStencilState;
    ds.DepthEnable    = desc->depth_test  ? TRUE : FALSE;
    ds.DepthWriteMask = desc->depth_write ? D3D12_DEPTH_WRITE_MASK_ALL
                                          : D3D12_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
    ds.StencilEnable  = FALSE;
    ds.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
    ds.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC default_stencil_op = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
        D3D12_COMPARISON_FUNC_ALWAYS
    };
    ds.FrontFace = default_stencil_op;
    ds.BackFace  = default_stencil_op;

    // Blend state — opaque default
    D3D12_BLEND_DESC& blend = pso_desc.BlendState;
    blend.AlphaToCoverageEnable  = FALSE;
    blend.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC default_rt_blend = {
        FALSE, FALSE,
        D3D12_BLEND_ONE,  D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE,  D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
    for (int i = 0; i < 8; ++i) {
        blend.RenderTarget[i] = default_rt_blend;
    }

    auto* pso_obj     = new PipelineObjectDX12();
    pso_obj->root_sig = dev->empty_root_sig.Get(); // borrowed ref
    // Store fine-grained topology for IASetPrimitiveTopology in SetPipeline
    pso_obj->topology = DXBTopologyToD3D(desc->topology);

    HRESULT hr = dev->device->CreateGraphicsPipelineState(
        &pso_desc, IID_PPV_ARGS(&pso_obj->pso));
    if (FAILED(hr)) {
        delete pso_obj;
        return ReturnHRESULT(dev, hr, "CreateGraphicsPipelineState");
    }

    *out = m_pipelines.Alloc(pso_obj, ObjectType::Pipeline);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyPipeline
// ---------------------------------------------------------------------------

void DX12Backend::DestroyPipeline(DXBPipeline pipeline) {
    auto* obj = m_pipelines.Get(pipeline, ObjectType::Pipeline);
    if (!obj) return;
    m_pipelines.Free(pipeline, ObjectType::Pipeline);
    delete obj;
}

} // namespace dxb
