package dxbridge;

import java.util.Arrays;
import java.util.List;

import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.LongByReference;

public interface DXBridgeLibrary extends Library {
    interface DXBLogCallback extends Callback {
        void invoke(int level, Pointer message, Pointer userData);
    }

    final class DXBAdapterInfo extends Structure {
        public int struct_version;
        public int index;
        public byte[] description = new byte[128];
        public long dedicated_video_mem;
        public long dedicated_system_mem;
        public long shared_system_mem;
        public int is_software;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList(
                "struct_version",
                "index",
                "description",
                "dedicated_video_mem",
                "dedicated_system_mem",
                "shared_system_mem",
                "is_software"
            );
        }
    }

    final class DXBDeviceDesc extends Structure {
        public int struct_version;
        public int backend;
        public int adapter_index;
        public int flags;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("struct_version", "backend", "adapter_index", "flags");
        }
    }

    final class DXBSwapChainDesc extends Structure {
        public int struct_version;
        public Pointer hwnd;
        public int width;
        public int height;
        public int format;
        public int buffer_count;
        public int flags;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList(
                "struct_version",
                "hwnd",
                "width",
                "height",
                "format",
                "buffer_count",
                "flags"
            );
        }
    }

    final class DXBBufferDesc extends Structure {
        public int struct_version;
        public int flags;
        public int byte_size;
        public int stride;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("struct_version", "flags", "byte_size", "stride");
        }
    }

    final class DXBShaderDesc extends Structure {
        public int struct_version;
        public int stage;
        public String source_code;
        public int source_size;
        public String source_name;
        public String entry_point;
        public String target;
        public int compile_flags;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList(
                "struct_version",
                "stage",
                "source_code",
                "source_size",
                "source_name",
                "entry_point",
                "target",
                "compile_flags"
            );
        }
    }

    final class DXBInputElementDesc extends Structure {
        public int struct_version;
        public int semantic;
        public int semantic_index;
        public int format;
        public int input_slot;
        public int byte_offset;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList(
                "struct_version",
                "semantic",
                "semantic_index",
                "format",
                "input_slot",
                "byte_offset"
            );
        }
    }

    final class DXBPipelineDesc extends Structure {
        public int struct_version;
        public long vs;
        public long ps;
        public long input_layout;
        public int topology;
        public int depth_test;
        public int depth_write;
        public int cull_back;
        public int wireframe;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList(
                "struct_version",
                "vs",
                "ps",
                "input_layout",
                "topology",
                "depth_test",
                "depth_write",
                "cull_back",
                "wireframe"
            );
        }
    }

    final class DXBViewport extends Structure {
        public float x;
        public float y;
        public float width;
        public float height;
        public float min_depth;
        public float max_depth;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("x", "y", "width", "height", "min_depth", "max_depth");
        }
    }

    void DXBridge_SetLogCallback(DXBLogCallback callback, Pointer userData);
    int DXBridge_Init(int backendHint);
    void DXBridge_Shutdown();
    int DXBridge_GetVersion();
    int DXBridge_SupportsFeature(int featureFlag);
    void DXBridge_GetLastError(byte[] buffer, int bufferSize);
    int DXBridge_GetLastHRESULT();
    int DXBridge_EnumerateAdapters(Pointer outBuffer, IntByReference inOutCount);
    int DXBridge_CreateDevice(DXBDeviceDesc desc, LongByReference outDevice);
    void DXBridge_DestroyDevice(long device);
    int DXBridge_GetFeatureLevel(long device);
    int DXBridge_CreateSwapChain(long device, DXBSwapChainDesc desc, LongByReference outSwapChain);
    void DXBridge_DestroySwapChain(long swapChain);
    int DXBridge_ResizeSwapChain(long swapChain, int width, int height);
    int DXBridge_GetBackBuffer(long swapChain, LongByReference outRenderTarget);
    int DXBridge_CreateRenderTargetView(long device, long renderTarget, LongByReference outRtv);
    void DXBridge_DestroyRTV(long rtv);
    int DXBridge_CompileShader(long device, DXBShaderDesc desc, LongByReference outShader);
    void DXBridge_DestroyShader(long shader);
    int DXBridge_CreateInputLayout(long device, Pointer elements, int elementCount, long vertexShader, LongByReference outLayout);
    void DXBridge_DestroyInputLayout(long layout);
    int DXBridge_CreatePipelineState(long device, DXBPipelineDesc desc, LongByReference outPipeline);
    void DXBridge_DestroyPipeline(long pipeline);
    int DXBridge_CreateBuffer(long device, DXBBufferDesc desc, LongByReference outBuffer);
    int DXBridge_UploadData(long buffer, Pointer data, int byteSize);
    void DXBridge_DestroyBuffer(long buffer);
    int DXBridge_BeginFrame(long device);
    int DXBridge_SetRenderTargets(long device, long rtv, long dsv);
    int DXBridge_ClearRenderTarget(long rtv, float[] rgba);
    int DXBridge_SetViewport(long device, DXBViewport viewport);
    int DXBridge_SetPipeline(long device, long pipeline);
    int DXBridge_SetVertexBuffer(long device, long buffer, int stride, int offset);
    int DXBridge_Draw(long device, int vertexCount, int startVertex);
    int DXBridge_EndFrame(long device);
    int DXBridge_Present(long swapChain, int syncInterval);
    Pointer DXBridge_GetNativeDevice(long device);
    Pointer DXBridge_GetNativeContext(long device);
}
