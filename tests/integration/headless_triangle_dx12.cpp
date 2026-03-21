// tests/integration/headless_triangle_dx12.cpp
// Headless (offscreen) WARP integration test for DXBridge DX12 backend.
// Requirements:
//   - No visible window
//   - WARP software adapter (always present on Windows 10/11)
//   - Renders a red triangle to a 256x256 offscreen render target
//   - Reads back the center pixel and verifies it is red (R > 0)
//   - Returns 0 (PASS) or 1 (FAIL)
//
// Escape hatch strategy (DX12):
//   GetNativeDevice() → ID3D12Device* (AddRef'd, caller must Release).
//   GetNativeContext() → nullptr (no immediate context in DX12).
//
//   Because DX12 has no immediate context, we cannot inject commands into the
//   DXBridge internal command list from outside. Instead this test:
//     - Uses DXBridge only for device init, adapter enumeration, shaders, and
//       the vertex buffer. All rendering and readback is done via raw D3D12
//       through the escape hatch.
//     - Creates a raw command queue, allocator, command list, fence, and an
//       offscreen RENDER_TARGET texture + READBACK buffer.
//     - Records render + CopyTextureRegion + readback in a single raw cmd list.
//
//   This avoids touching the DXBridge swap chain or frame loop at all.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "dxbridge/dxbridge.h"

#include <cstdio>
#include <cstring>

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// HLSL shaders
// ---------------------------------------------------------------------------

static const char* k_vs_src =
    "float4 main(float2 pos : POSITION) : SV_POSITION {\n"
    "    return float4(pos, 0.0f, 1.0f);\n"
    "}\n";

static const char* k_ps_src =
    "float4 main() : SV_TARGET {\n"
    "    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
    "}\n";

// Vertex data: triangle covering center (NDC coords), float2 POSITION
static const float k_verts[] = {
     0.0f,  0.5f,   // top center
    -0.5f, -0.5f,   // bottom left
     0.5f, -0.5f,   // bottom right
};

// ---------------------------------------------------------------------------
// Helper: fail with message
// ---------------------------------------------------------------------------

static int fail(const char* reason) {
    printf("FAIL: %s\n", reason);
    DXBridge_Shutdown();
    return 1;
}

// ---------------------------------------------------------------------------
// Helper: resource barrier (raw D3D12)
// ---------------------------------------------------------------------------

static void RawTransition(ID3D12GraphicsCommandList* list,
                           ID3D12Resource*            resource,
                           D3D12_RESOURCE_STATES      from,
                           D3D12_RESOURCE_STATES      to) noexcept
{
    D3D12_RESOURCE_BARRIER b = {};
    b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource   = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter  = to;
    list->ResourceBarrier(1, &b);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("=== DXBridge Headless Triangle DX12 (WARP) ===\n");

    // ------------------------------------------------------------------
    // 1. Init DXBridge
    // ------------------------------------------------------------------
    DXBResult r = DXBridge_Init(DXB_BACKEND_DX12);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        printf("SKIP: DXBridge_Init failed: %s (HRESULT 0x%08X)\n",
               err, DXBridge_GetLastHRESULT());
        return 0;
    }

    // ------------------------------------------------------------------
    // 2. Enumerate adapters — find WARP (software) adapter
    // ------------------------------------------------------------------
    uint32_t adapter_count = 0;
    DXBridge_EnumerateAdapters(nullptr, &adapter_count);
    if (adapter_count == 0) return fail("No adapters found");

    DXBAdapterInfo* adapters = new DXBAdapterInfo[adapter_count];
    for (uint32_t i = 0; i < adapter_count; ++i)
        adapters[i].struct_version = DXBRIDGE_STRUCT_VERSION;

    r = DXBridge_EnumerateAdapters(adapters, &adapter_count);
    if (r != DXB_OK) { delete[] adapters; return fail("EnumerateAdapters failed"); }

    uint32_t warp_index = 0xFFFFFFFF;
    for (uint32_t i = 0; i < adapter_count; ++i) {
        printf("  Adapter %u: %s  software=%u\n",
               i, adapters[i].description, adapters[i].is_software);
        if (adapters[i].is_software && warp_index == 0xFFFFFFFF)
            warp_index = i;
    }
    delete[] adapters;

    if (warp_index == 0xFFFFFFFF) {
        printf("SKIP: No WARP adapter found.\n");
        DXBridge_Shutdown();
        return 0;
    }
    printf("  Using WARP adapter index %u\n", warp_index);

    // ------------------------------------------------------------------
    // 3. Create device on WARP adapter
    // ------------------------------------------------------------------
    DXBDeviceDesc dev_desc = {};
    dev_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    dev_desc.backend        = DXB_BACKEND_DX12;
    dev_desc.adapter_index  = warp_index;
    dev_desc.flags          = DXB_DEVICE_FLAG_DEBUG;

    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CreateDevice(&dev_desc, &device);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        printf("SKIP: CreateDevice failed: %s\n", err);
        DXBridge_Shutdown();
        return 0;
    }
    printf("  Device created (feature level 0x%04X)\n",
           DXBridge_GetFeatureLevel(device));

    // Get raw ID3D12Device* via escape hatch (AddRef'd — we must Release)
    auto* raw_device = static_cast<ID3D12Device*>(DXBridge_GetNativeDevice(device));
    if (!raw_device) return fail("GetNativeDevice returned null");

    // ------------------------------------------------------------------
    // 4. Create raw GPU resources via escape hatch:
    //    - offscreen RENDER_TARGET texture (256x256 RGBA8)
    //    - READBACK buffer (for pixel readback after render)
    //    - raw command queue, allocator, command list, fence
    // ------------------------------------------------------------------
    const uint32_t RT_W = 256;
    const uint32_t RT_H = 256;
    // Row pitch must be aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256 bytes)
    const UINT row_pitch_aligned = ((RT_W * 4 + 255) / 256) * 256;
    const UINT readback_size     = row_pitch_aligned * RT_H;

    // Offscreen render target texture
    ComPtr<ID3D12Resource> rt_tex;
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width            = RT_W;
        rd.Height           = RT_H;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rd.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE cv = {};
        cv.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
        // color = {0,0,0,1} (black, opaque)

        HRESULT hr = raw_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &cv,
            IID_PPV_ARGS(rt_tex.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommittedResource(rt_tex) 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
    }

    // Readback buffer
    ComPtr<ID3D12Resource> readback_buf;
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = readback_size;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = raw_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(readback_buf.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommittedResource(readback) 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
    }

    // RTV descriptor heap (1 slot)
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  rtv_cpu = {};
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 1;
        hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = raw_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(rtv_heap.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateDescriptorHeap(rtv) 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
        rtv_cpu = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_RENDER_TARGET_VIEW_DESC rv = {};
        rv.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        raw_device->CreateRenderTargetView(rt_tex.Get(), &rv, rtv_cpu);
    }

    // Raw command queue (DIRECT)
    ComPtr<ID3D12CommandQueue> raw_queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        HRESULT hr = raw_device->CreateCommandQueue(&qd, IID_PPV_ARGS(raw_queue.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommandQueue 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
    }

    // Raw command allocator + list
    ComPtr<ID3D12CommandAllocator>    raw_alloc;
    ComPtr<ID3D12GraphicsCommandList> raw_list;
    {
        HRESULT hr = raw_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(raw_alloc.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommandAllocator 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
        hr = raw_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, raw_alloc.Get(), nullptr, IID_PPV_ARGS(raw_list.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommandList 0x%08X\n", (unsigned)hr); raw_device->Release(); return 1; }
        // CreateCommandList starts in recording state — ready to record
    }

    // Fence for GPU synchronisation
    ComPtr<ID3D12Fence> raw_fence;
    HANDLE              raw_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    {
        HRESULT hr = raw_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(raw_fence.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateFence 0x%08X\n", (unsigned)hr); CloseHandle(raw_event); raw_device->Release(); return 1; }
    }

    // ------------------------------------------------------------------
    // 5. Build empty root signature and raw PSO
    //    (Matches the empty root signature used by the DXBridge DX12 backend)
    // ------------------------------------------------------------------
    ComPtr<ID3D12RootSignature> raw_root_sig;
    {
        D3D12_ROOT_SIGNATURE_DESC rsd = {};
        rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> sig_blob, err_blob;
        HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 sig_blob.ReleaseAndGetAddressOf(),
                                                 err_blob.ReleaseAndGetAddressOf());
        if (FAILED(hr)) { printf("FAIL: D3D12SerializeRootSignature 0x%08X\n", (unsigned)hr); CloseHandle(raw_event); raw_device->Release(); return 1; }
        hr = raw_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(),
                                              IID_PPV_ARGS(raw_root_sig.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateRootSignature 0x%08X\n", (unsigned)hr); CloseHandle(raw_event); raw_device->Release(); return 1; }
    }

    // Compile VS and PS blobs
    ComPtr<ID3DBlob> vs_blob, ps_blob;
    {
        ComPtr<ID3DBlob> err;
        HRESULT hr = D3DCompile(k_vs_src, strlen(k_vs_src), "vs", nullptr, nullptr,
                                "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                vs_blob.ReleaseAndGetAddressOf(), err.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("FAIL: D3DCompile(VS): %s\n", err ? (char*)err->GetBufferPointer() : "?");
            CloseHandle(raw_event); raw_device->Release(); return 1;
        }
        hr = D3DCompile(k_ps_src, strlen(k_ps_src), "ps", nullptr, nullptr,
                        "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                        ps_blob.ReleaseAndGetAddressOf(), err.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("FAIL: D3DCompile(PS): %s\n", err ? (char*)err->GetBufferPointer() : "?");
            CloseHandle(raw_event); raw_device->Release(); return 1;
        }
    }

    // Create raw PSO
    ComPtr<ID3D12PipelineState> raw_pso;
    {
        D3D12_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_RASTERIZER_DESC rs = {};
        rs.FillMode        = D3D12_FILL_MODE_SOLID;
        rs.CullMode        = D3D12_CULL_MODE_NONE;
        rs.DepthClipEnable = TRUE;

        D3D12_BLEND_DESC blend = {};
        blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_DEPTH_STENCIL_DESC ds = {};
        ds.DepthEnable    = FALSE;
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
        pd.pRootSignature        = raw_root_sig.Get();
        pd.VS                    = { vs_blob->GetBufferPointer(), vs_blob->GetBufferSize() };
        pd.PS                    = { ps_blob->GetBufferPointer(), ps_blob->GetBufferSize() };
        pd.InputLayout           = { il, 1 };
        pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets      = 1;
        pd.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
        pd.DSVFormat             = DXGI_FORMAT_UNKNOWN;
        pd.SampleMask            = UINT_MAX;
        pd.SampleDesc            = { 1, 0 };
        pd.RasterizerState       = rs;
        pd.BlendState            = blend;
        pd.DepthStencilState     = ds;

        HRESULT hr = raw_device->CreateGraphicsPipelineState(&pd,
                                                              IID_PPV_ARGS(raw_pso.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            printf("FAIL: CreateGraphicsPipelineState 0x%08X\n", (unsigned)hr);
            CloseHandle(raw_event); raw_device->Release(); return 1;
        }
    }

    // Raw upload vertex buffer
    ComPtr<ID3D12Resource> raw_vb;
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = sizeof(k_verts);
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = raw_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(raw_vb.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) { printf("FAIL: CreateCommittedResource(vb) 0x%08X\n", (unsigned)hr); CloseHandle(raw_event); raw_device->Release(); return 1; }

        void* mapped = nullptr;
        D3D12_RANGE zero_range = {};
        hr = raw_vb->Map(0, &zero_range, &mapped);
        if (FAILED(hr)) { printf("FAIL: vb->Map 0x%08X\n", (unsigned)hr); CloseHandle(raw_event); raw_device->Release(); return 1; }
        memcpy(mapped, k_verts, sizeof(k_verts));
        raw_vb->Unmap(0, nullptr);
    }

    // ------------------------------------------------------------------
    // 6. Record render + readback commands into raw_list
    // ------------------------------------------------------------------
    {
        // rt_tex starts in RENDER_TARGET state (initial state from CreateCommittedResource)

        // Clear + draw
        raw_list->OMSetRenderTargets(1, &rtv_cpu, FALSE, nullptr);

        float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        raw_list->ClearRenderTargetView(rtv_cpu, black, 0, nullptr);

        D3D12_VIEWPORT vp = { 0.0f, 0.0f,
                               static_cast<float>(RT_W),
                               static_cast<float>(RT_H),
                               0.0f, 1.0f };
        raw_list->RSSetViewports(1, &vp);

        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(RT_W), static_cast<LONG>(RT_H) };
        raw_list->RSSetScissorRects(1, &scissor);

        raw_list->SetGraphicsRootSignature(raw_root_sig.Get());
        raw_list->SetPipelineState(raw_pso.Get());
        raw_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = raw_vb->GetGPUVirtualAddress();
        vbv.SizeInBytes    = sizeof(k_verts);
        vbv.StrideInBytes  = sizeof(float) * 2;
        raw_list->IASetVertexBuffers(0, 1, &vbv);
        raw_list->DrawInstanced(3, 1, 0, 0);

        // Transition RENDER_TARGET → COPY_SOURCE for readback
        RawTransition(raw_list.Get(), rt_tex.Get(),
                      D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_COPY_SOURCE);

        // CopyTextureRegion to readback buffer
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource        = rt_tex.Get();
        src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource                            = readback_buf.Get();
        dst.Type                                 = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Offset               = 0;
        dst.PlacedFootprint.Footprint.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
        dst.PlacedFootprint.Footprint.Width      = RT_W;
        dst.PlacedFootprint.Footprint.Height     = RT_H;
        dst.PlacedFootprint.Footprint.Depth      = 1;
        dst.PlacedFootprint.Footprint.RowPitch   = row_pitch_aligned;

        raw_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        HRESULT hr = raw_list->Close();
        if (FAILED(hr)) {
            printf("FAIL: raw_list->Close 0x%08X\n", (unsigned)hr);
            CloseHandle(raw_event); raw_device->Release();
            DXBridge_DestroyDevice(device); DXBridge_Shutdown(); return 1;
        }
    }

    // ------------------------------------------------------------------
    // 7. Execute raw_list and wait for GPU completion
    // ------------------------------------------------------------------
    {
        ID3D12CommandList* lists[] = { raw_list.Get() };
        raw_queue->ExecuteCommandLists(1, lists);
        raw_queue->Signal(raw_fence.Get(), 1);
        raw_fence->SetEventOnCompletion(1, raw_event);
        WaitForSingleObject(raw_event, INFINITE);
        CloseHandle(raw_event);
        raw_event = nullptr;
    }

    // ------------------------------------------------------------------
    // 8. Map readback buffer — read center pixel (128, 128)
    // ------------------------------------------------------------------
    uint8_t pR = 0, pG = 0, pB = 0, pA = 0;
    {
        void* mapped = nullptr;
        D3D12_RANGE read_range = { 0, readback_size };
        HRESULT hr = readback_buf->Map(0, &read_range, &mapped);
        if (FAILED(hr)) {
            printf("FAIL: Map readback 0x%08X\n", (unsigned)hr);
            raw_device->Release();
            DXBridge_DestroyDevice(device); DXBridge_Shutdown(); return 1;
        }

        const uint32_t cx = RT_W / 2;
        const uint32_t cy = RT_H / 2;
        const uint8_t* row   = reinterpret_cast<const uint8_t*>(mapped)
                               + cy * row_pitch_aligned;
        const uint8_t* pixel = row + cx * 4;
        pR = pixel[0];
        pG = pixel[1];
        pB = pixel[2];
        pA = pixel[3];

        D3D12_RANGE write_range = { 0, 0 };
        readback_buf->Unmap(0, &write_range);
    }

    printf("  Center pixel (R=%u G=%u B=%u A=%u)\n", pR, pG, pB, pA);

    // ------------------------------------------------------------------
    // 9. Cleanup
    // ------------------------------------------------------------------
    raw_device->Release();

    DXBridge_DestroyDevice(device);
    DXBridge_Shutdown();

    // ------------------------------------------------------------------
    // 10. Verify result
    // ------------------------------------------------------------------
    if (pR == 0) {
        printf("FAIL: Center pixel red channel is 0 (expected > 0 for red triangle)\n");
        return 1;
    }

    printf("PASS\n");
    return 0;
}
