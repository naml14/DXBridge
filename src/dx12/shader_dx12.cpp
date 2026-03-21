// src/dx12/shader_dx12.cpp
// DX12Backend — shader compilation via D3DCompile (DXBC SM5.0).
// Phase C.2: CompileShader, DestroyShader.
#include "device_dx12.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace {

static DXBResult GuardDeviceLost(dxb::DeviceObjectDX12* dev, const char* context) noexcept {
    if (!dev || !dev->device_lost) {
        return DXB_OK;
    }

    dxb::SetLastError(DXB_E_DEVICE_LOST, "%s: device lost", context);
    return DXB_E_DEVICE_LOST;
}

} // namespace

namespace dxb {

// ---------------------------------------------------------------------------
// CompileShader — D3DCompile primary compiler (DXBC, Shader Model 5.0)
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CompileShader(DXBDevice device_handle,
                                      const DXBShaderDesc* desc,
                                      DXBShader* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CompileShader: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    // Shader compilation is CPU-side, but sticky device-lost still applies
    // because the API takes an owning device handle.
    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CompileShader: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CompileShader");
    if (lost != DXB_OK) {
        return lost;
    }

    if (!desc->source_code || !desc->entry_point || !desc->target) {
        SetLastError(DXB_E_INVALID_ARG, "CompileShader: source_code/entry_point/target must not be null");
        return DXB_E_INVALID_ARG;
    }

    SIZE_T source_size = desc->source_size
                         ? static_cast<SIZE_T>(desc->source_size)
                         : strlen(desc->source_code);

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (desc->compile_flags & 0x1) {
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }
#ifdef NDEBUG
    // Release builds: optimization
    if (!(desc->compile_flags & 0x1)) {
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    }
#endif

    ComPtr<ID3DBlob> bytecode_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(
        desc->source_code,
        source_size,
        desc->source_name,       // filename for error messages (can be null)
        nullptr,                 // no defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        desc->entry_point,
        desc->target,            // e.g. "vs_5_0" / "ps_5_0"
        flags, 0,
        bytecode_blob.ReleaseAndGetAddressOf(),
        error_blob.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        if (error_blob && error_blob->GetBufferSize() > 0) {
            SetLastError(DXB_E_SHADER_COMPILE, "D3DCompile: %s",
                         static_cast<const char*>(error_blob->GetBufferPointer()));
        } else {
            SetLastErrorFromHR(hr, "D3DCompile");
        }
        return DXB_E_SHADER_COMPILE;
    }

    // Copy bytecode out of the ID3DBlob into a std::vector for ownership
    auto* shader_obj = new ShaderObjectDX12();
    shader_obj->stage = desc->stage;
    shader_obj->bytecode.assign(
        static_cast<uint8_t*>(bytecode_blob->GetBufferPointer()),
        static_cast<uint8_t*>(bytecode_blob->GetBufferPointer()) + bytecode_blob->GetBufferSize());

    *out = m_shaders.Alloc(shader_obj, ObjectType::Shader);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyShader
// ---------------------------------------------------------------------------

void DX12Backend::DestroyShader(DXBShader shader) {
    auto* obj = m_shaders.Get(shader, ObjectType::Shader);
    if (!obj) return;
    m_shaders.Free(shader, ObjectType::Shader);
    delete obj;
}

} // namespace dxb
