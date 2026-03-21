// src/dx11/shader_dx11.cpp
// DX11Backend — HLSL shader compilation via D3DCompile.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// CompileShader
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CompileShader(DXBDevice device,
                                      const DXBShaderDesc* desc,
                                      DXBShader* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CompileShader: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    if (!desc->source_code || !desc->entry_point || !desc->target) {
        SetLastError(DXB_E_INVALID_ARG,
                     "CompileShader: source_code, entry_point, and target must not be null");
        return DXB_E_INVALID_ARG;
    }

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CompileShader: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    SIZE_T source_size = (desc->source_size > 0)
        ? static_cast<SIZE_T>(desc->source_size)
        : strlen(desc->source_code);

    // Compile flags
    UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (desc->compile_flags & 0x1) { // DXB_COMPILE_DEBUG
        compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }
#if defined(_DEBUG) || defined(DEBUG)
    compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> code_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(
        desc->source_code,
        source_size,
        desc->source_name ? desc->source_name : "<dxbridge>",
        nullptr,            // no defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        desc->entry_point,
        desc->target,
        compile_flags,
        0,                  // no effect flags
        code_blob.ReleaseAndGetAddressOf(),
        error_blob.ReleaseAndGetAddressOf()
    );

    if (FAILED(hr)) {
        if (error_blob && error_blob->GetBufferSize() > 0) {
            SetLastError(DXB_E_SHADER_COMPILE,
                         "Shader compile error: %s",
                         static_cast<const char*>(error_blob->GetBufferPointer()));
        } else {
            SetLastErrorFromHR(hr, "D3DCompile");
        }
        return DXB_E_SHADER_COMPILE;
    }

    // Log any warnings
    if (error_blob && error_blob->GetBufferSize() > 0) {
        DXB_LOG_WARN("Shader compile warning: %s",
                     static_cast<const char*>(error_blob->GetBufferPointer()));
    }

    // Keep bytecode for CreateInputLayout
    const uint8_t* bytecode_ptr = static_cast<const uint8_t*>(code_blob->GetBufferPointer());
    size_t bytecode_size        = code_blob->GetBufferSize();

    auto* shader_obj = new ShaderObjectDX11();
    shader_obj->stage    = desc->stage;
    shader_obj->bytecode.assign(bytecode_ptr, bytecode_ptr + bytecode_size);

    if (desc->stage == DXB_SHADER_STAGE_VERTEX) {
        hr = dev_obj->device->CreateVertexShader(
            code_blob->GetBufferPointer(),
            code_blob->GetBufferSize(),
            nullptr,
            shader_obj->vs.ReleaseAndGetAddressOf());
    } else {
        hr = dev_obj->device->CreatePixelShader(
            code_blob->GetBufferPointer(),
            code_blob->GetBufferSize(),
            nullptr,
            shader_obj->ps.ReleaseAndGetAddressOf());
    }

    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::Create*Shader");
        delete shader_obj;
        return MapHRESULT(hr);
    }

    *out = m_shaders.Alloc(shader_obj, ObjectType::Shader);
    return DXB_OK;
}

void DX11Backend::DestroyShader(DXBShader shader) {
    auto* obj = m_shaders.Get(shader, ObjectType::Shader);
    if (!obj) return;
    m_shaders.Free(shader, ObjectType::Shader);
    delete obj;
}

} // namespace dxb
