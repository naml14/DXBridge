// src/common/format_util.hpp
// Shared format / topology converters used by both DX11 and DX12 backends.
#pragma once
#include <dxgi.h>
#include <d3d12.h>
#include "../../include/dxbridge/dxbridge.h"

namespace dxb {

inline DXGI_FORMAT DXBFormatToDXGI(DXBFormat fmt) noexcept {
    switch (fmt) {
    case DXB_FORMAT_RGBA8_UNORM:    return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXB_FORMAT_RGBA16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXB_FORMAT_D32_FLOAT:      return DXGI_FORMAT_D32_FLOAT;
    case DXB_FORMAT_D24_UNORM_S8:   return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case DXB_FORMAT_R32_UINT:       return DXGI_FORMAT_R32_UINT;
    case DXB_FORMAT_RG32_FLOAT:     return DXGI_FORMAT_R32G32_FLOAT;
    case DXB_FORMAT_RGB32_FLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
    case DXB_FORMAT_RGBA32_FLOAT:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

// Map DXBFormat to typeless equivalent (for depth texture SRVs).
inline DXGI_FORMAT DXBFormatToTypeless(DXBFormat fmt) noexcept {
    switch (fmt) {
    case DXB_FORMAT_D32_FLOAT:    return DXGI_FORMAT_R32_TYPELESS;
    case DXB_FORMAT_D24_UNORM_S8: return DXGI_FORMAT_R24G8_TYPELESS;
    default:                      return DXBFormatToDXGI(fmt);
    }
}

inline const char* DXBSemanticToString(DXBInputSemantic sem) noexcept {
    switch (sem) {
    case DXB_SEMANTIC_POSITION: return "POSITION";
    case DXB_SEMANTIC_NORMAL:   return "NORMAL";
    case DXB_SEMANTIC_TEXCOORD: return "TEXCOORD";
    case DXB_SEMANTIC_COLOR:    return "COLOR";
    default:                    return "UNKNOWN";
    }
}

// Topology type for D3D12_GRAPHICS_PIPELINE_STATE_DESC::PrimitiveTopologyType.
inline D3D12_PRIMITIVE_TOPOLOGY_TYPE DXBTopologyToD3D12Type(DXBPrimTopology t) noexcept {
    switch (t) {
    case DXB_PRIM_TRIANGLELIST:
    case DXB_PRIM_TRIANGLESTRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case DXB_PRIM_LINELIST:      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case DXB_PRIM_POINTLIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default:                     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

// Topology for IASetPrimitiveTopology (same D3D_PRIMITIVE_TOPOLOGY type used by both DX11 and DX12).
inline D3D_PRIMITIVE_TOPOLOGY DXBTopologyToD3D(DXBPrimTopology t) noexcept {
    switch (t) {
    case DXB_PRIM_TRIANGLELIST:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case DXB_PRIM_TRIANGLESTRIP: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case DXB_PRIM_LINELIST:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case DXB_PRIM_POINTLIST:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:                     return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

} // namespace dxb
