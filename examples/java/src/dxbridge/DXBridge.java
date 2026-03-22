package dxbridge;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.sun.jna.Library;
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.LongByReference;

public final class DXBridge {
    public interface LogHandler {
        void onLog(int level, String message, Pointer userData);
    }

    public static final long DXBRIDGE_NULL_HANDLE = 0L;
    public static final int DXBRIDGE_STRUCT_VERSION = 1;

    public static final int DXB_OK = 0;
    public static final int DXB_E_INVALID_HANDLE = -1;
    public static final int DXB_E_DEVICE_LOST = -2;
    public static final int DXB_E_OUT_OF_MEMORY = -3;
    public static final int DXB_E_INVALID_ARG = -4;
    public static final int DXB_E_NOT_SUPPORTED = -5;
    public static final int DXB_E_SHADER_COMPILE = -6;
    public static final int DXB_E_NOT_INITIALIZED = -7;
    public static final int DXB_E_INVALID_STATE = -8;
    public static final int DXB_E_INTERNAL = -999;

    public static final int DXB_BACKEND_AUTO = 0;
    public static final int DXB_BACKEND_DX11 = 1;
    public static final int DXB_BACKEND_DX12 = 2;

    public static final int DXB_FORMAT_UNKNOWN = 0;
    public static final int DXB_FORMAT_RGBA8_UNORM = 1;
    public static final int DXB_FORMAT_D32_FLOAT = 3;
    public static final int DXB_FORMAT_RG32_FLOAT = 6;
    public static final int DXB_FORMAT_RGB32_FLOAT = 7;

    public static final int DXB_DEVICE_FLAG_DEBUG = 0x0001;
    public static final int DXB_DEVICE_FLAG_GPU_VALIDATION = 0x0002;

    public static final int DXB_BUFFER_FLAG_VERTEX = 0x0001;
    public static final int DXB_BUFFER_FLAG_DYNAMIC = 0x0008;

    public static final int DXB_SHADER_STAGE_VERTEX = 0;
    public static final int DXB_SHADER_STAGE_PIXEL = 1;

    public static final int DXB_PRIM_TRIANGLELIST = 0;

    public static final int DXB_SEMANTIC_POSITION = 0;
    public static final int DXB_SEMANTIC_COLOR = 3;

    public static final int DXB_COMPILE_DEBUG = 0x0001;
    public static final int DXB_FEATURE_DX12 = 0x0001;

    private static final Charset UTF8 = Charset.forName("UTF-8");

    private final File dllPath;
    private final DXBridgeLibrary api;
    private DXBridgeLibrary.DXBLogCallback callbackRef;

    public DXBridge(String explicitPath) {
        this.dllPath = locateDll(explicitPath);

        Map<String, Object> options = new HashMap<String, Object>();
        options.put(Library.OPTION_STRING_ENCODING, "UTF-8");
        this.api = Native.load(this.dllPath.getAbsolutePath(), DXBridgeLibrary.class, options);
    }

    public File getDllPath() {
        return dllPath;
    }

    public DXBridgeLibrary api() {
        return api;
    }

    public void setLogCallback(final LogHandler handler) {
        if (handler == null) {
            callbackRef = null;
            api.DXBridge_SetLogCallback(null, null);
            return;
        }

        callbackRef = new DXBridgeLibrary.DXBLogCallback() {
            @Override
            public void invoke(int level, Pointer message, Pointer userData) {
                handler.onLog(level, pointerToUtf8(message), userData);
            }
        };
        api.DXBridge_SetLogCallback(callbackRef, null);
    }

    public int getVersion() {
        return api.DXBridge_GetVersion();
    }

    public void init(int backend) {
        requireOk(api.DXBridge_Init(backend), "DXBridge_Init");
    }

    public void shutdown() {
        api.DXBridge_Shutdown();
    }

    public boolean supportsFeature(int featureFlag) {
        return api.DXBridge_SupportsFeature(featureFlag) != 0;
    }

    public String getLastError() {
        byte[] buffer = new byte[512];
        api.DXBridge_GetLastError(buffer, buffer.length);
        return zeroTerminatedUtf8(buffer);
    }

    public int getLastHRESULT() {
        return api.DXBridge_GetLastHRESULT();
    }

    public String describeFailure(int result) {
        String message = getLastError();
        String hresult = formatHresult(getLastHRESULT());
        String name = resultName(result);
        if (message.length() == 0) {
            return name + " (HRESULT " + hresult + ")";
        }
        return name + ": " + message + " (HRESULT " + hresult + ")";
    }

    public void requireOk(int result, String functionName) {
        if (result != DXB_OK) {
            throw new DXBridgeException(functionName + " failed: " + describeFailure(result));
        }
    }

    public List<DXBridgeLibrary.DXBAdapterInfo> enumerateAdapters() {
        IntByReference count = new IntByReference(0);
        requireOk(api.DXBridge_EnumerateAdapters(Pointer.NULL, count), "DXBridge_EnumerateAdapters(count)");
        if (count.getValue() == 0) {
            return Collections.emptyList();
        }

        DXBridgeLibrary.DXBAdapterInfo[] adapters = newAdapterArray(count.getValue());
        requireOk(
            api.DXBridge_EnumerateAdapters(adapters[0].getPointer(), count),
            "DXBridge_EnumerateAdapters(fill)"
        );
        for (int i = 0; i < count.getValue(); i++) {
            adapters[i].read();
        }

        List<DXBridgeLibrary.DXBAdapterInfo> result = new ArrayList<DXBridgeLibrary.DXBAdapterInfo>();
        for (int i = 0; i < count.getValue(); i++) {
            result.add(adapters[i]);
        }
        return result;
    }

    public long createDevice(int backend, int adapterIndex, int flags) {
        DXBridgeLibrary.DXBDeviceDesc desc = new DXBridgeLibrary.DXBDeviceDesc();
        desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        desc.backend = backend;
        desc.adapter_index = adapterIndex;
        desc.flags = flags;
        desc.write();

        LongByReference outDevice = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_CreateDevice(desc, outDevice), "DXBridge_CreateDevice");
        return outDevice.getValue();
    }

    public void destroyDevice(long device) {
        api.DXBridge_DestroyDevice(device);
    }

    public int getFeatureLevel(long device) {
        return api.DXBridge_GetFeatureLevel(device);
    }

    public long createSwapChain(long device, Pointer hwnd, int width, int height, int format, int bufferCount, int flags) {
        DXBridgeLibrary.DXBSwapChainDesc desc = new DXBridgeLibrary.DXBSwapChainDesc();
        desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        desc.hwnd = hwnd;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        desc.buffer_count = bufferCount;
        desc.flags = flags;
        desc.write();

        LongByReference outSwapChain = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_CreateSwapChain(device, desc, outSwapChain), "DXBridge_CreateSwapChain");
        return outSwapChain.getValue();
    }

    public void destroySwapChain(long swapChain) {
        api.DXBridge_DestroySwapChain(swapChain);
    }

    public void resizeSwapChain(long swapChain, int width, int height) {
        requireOk(api.DXBridge_ResizeSwapChain(swapChain, width, height), "DXBridge_ResizeSwapChain");
    }

    public long getBackBuffer(long swapChain) {
        LongByReference outRenderTarget = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_GetBackBuffer(swapChain, outRenderTarget), "DXBridge_GetBackBuffer");
        return outRenderTarget.getValue();
    }

    public long createRenderTargetView(long device, long renderTarget) {
        LongByReference outRtv = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(
            api.DXBridge_CreateRenderTargetView(device, renderTarget, outRtv),
            "DXBridge_CreateRenderTargetView"
        );
        return outRtv.getValue();
    }

    public void destroyRenderTargetView(long rtv) {
        api.DXBridge_DestroyRTV(rtv);
    }

    public long compileShader(long device, int stage, String sourceCode, String sourceName, String entryPoint, String target, int compileFlags) {
        DXBridgeLibrary.DXBShaderDesc desc = new DXBridgeLibrary.DXBShaderDesc();
        desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        desc.stage = stage;
        desc.source_code = sourceCode;
        desc.source_size = 0;
        desc.source_name = sourceName;
        desc.entry_point = entryPoint;
        desc.target = target;
        desc.compile_flags = compileFlags;
        desc.write();

        LongByReference outShader = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_CompileShader(device, desc, outShader), "DXBridge_CompileShader");
        return outShader.getValue();
    }

    public void destroyShader(long shader) {
        api.DXBridge_DestroyShader(shader);
    }

    public long createInputLayout(long device, DXBridgeLibrary.DXBInputElementDesc[] elements, long vertexShader) {
        if (elements == null || elements.length == 0) {
            throw new IllegalArgumentException("elements must not be empty");
        }

        for (int i = 0; i < elements.length; i++) {
            elements[i].struct_version = DXBRIDGE_STRUCT_VERSION;
            elements[i].write();
        }

        LongByReference outLayout = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(
            api.DXBridge_CreateInputLayout(device, elements[0].getPointer(), elements.length, vertexShader, outLayout),
            "DXBridge_CreateInputLayout"
        );
        return outLayout.getValue();
    }

    public void destroyInputLayout(long layout) {
        api.DXBridge_DestroyInputLayout(layout);
    }

    public long createPipelineState(long device, long vertexShader, long pixelShader, long inputLayout, int topology, int depthTest, int depthWrite, int cullBack, int wireframe) {
        DXBridgeLibrary.DXBPipelineDesc desc = new DXBridgeLibrary.DXBPipelineDesc();
        desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        desc.vs = vertexShader;
        desc.ps = pixelShader;
        desc.input_layout = inputLayout;
        desc.topology = topology;
        desc.depth_test = depthTest;
        desc.depth_write = depthWrite;
        desc.cull_back = cullBack;
        desc.wireframe = wireframe;
        desc.write();

        LongByReference outPipeline = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_CreatePipelineState(device, desc, outPipeline), "DXBridge_CreatePipelineState");
        return outPipeline.getValue();
    }

    public void destroyPipeline(long pipeline) {
        api.DXBridge_DestroyPipeline(pipeline);
    }

    public long createBuffer(long device, int flags, int byteSize, int stride) {
        DXBridgeLibrary.DXBBufferDesc desc = new DXBridgeLibrary.DXBBufferDesc();
        desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        desc.flags = flags;
        desc.byte_size = byteSize;
        desc.stride = stride;
        desc.write();

        LongByReference outBuffer = new LongByReference(DXBRIDGE_NULL_HANDLE);
        requireOk(api.DXBridge_CreateBuffer(device, desc, outBuffer), "DXBridge_CreateBuffer");
        return outBuffer.getValue();
    }

    public void uploadFloats(long buffer, float[] data) {
        Memory memory = new Memory((long) data.length * 4L);
        for (int i = 0; i < data.length; i++) {
            memory.setFloat((long) i * 4L, data[i]);
        }
        requireOk(api.DXBridge_UploadData(buffer, memory, (int) memory.size()), "DXBridge_UploadData");
    }

    public void destroyBuffer(long buffer) {
        api.DXBridge_DestroyBuffer(buffer);
    }

    public void beginFrame(long device) {
        requireOk(api.DXBridge_BeginFrame(device), "DXBridge_BeginFrame");
    }

    public void setRenderTargets(long device, long rtv, long dsv) {
        requireOk(api.DXBridge_SetRenderTargets(device, rtv, dsv), "DXBridge_SetRenderTargets");
    }

    public void clearRenderTarget(long rtv, float[] rgba) {
        requireOk(api.DXBridge_ClearRenderTarget(rtv, rgba), "DXBridge_ClearRenderTarget");
    }

    public void setViewport(long device, float x, float y, float width, float height, float minDepth, float maxDepth) {
        DXBridgeLibrary.DXBViewport viewport = new DXBridgeLibrary.DXBViewport();
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.min_depth = minDepth;
        viewport.max_depth = maxDepth;
        viewport.write();
        requireOk(api.DXBridge_SetViewport(device, viewport), "DXBridge_SetViewport");
    }

    public void setPipeline(long device, long pipeline) {
        requireOk(api.DXBridge_SetPipeline(device, pipeline), "DXBridge_SetPipeline");
    }

    public void setVertexBuffer(long device, long buffer, int stride, int offset) {
        requireOk(api.DXBridge_SetVertexBuffer(device, buffer, stride, offset), "DXBridge_SetVertexBuffer");
    }

    public void draw(long device, int vertexCount, int startVertex) {
        requireOk(api.DXBridge_Draw(device, vertexCount, startVertex), "DXBridge_Draw");
    }

    public void endFrame(long device) {
        requireOk(api.DXBridge_EndFrame(device), "DXBridge_EndFrame");
    }

    public void present(long swapChain, int syncInterval) {
        requireOk(api.DXBridge_Present(swapChain, syncInterval), "DXBridge_Present");
    }

    public static String parseVersion(int version) {
        int major = (version >>> 16) & 0xFF;
        int minor = (version >>> 8) & 0xFF;
        int patch = version & 0xFF;
        return major + "." + minor + "." + patch;
    }

    public static String formatHresult(int value) {
        return String.format(Locale.US, "0x%08X", value);
    }

    public static String formatBytes(long value) {
        String[] units = { "B", "KiB", "MiB", "GiB", "TiB" };
        double amount = (double) value;
        int unitIndex = 0;
        while (amount >= 1024.0 && unitIndex < units.length - 1) {
            amount /= 1024.0;
            unitIndex++;
        }
        if (unitIndex == 0) {
            return Long.toString(value) + " " + units[unitIndex];
        }
        return String.format(Locale.US, "%.1f %s", amount, units[unitIndex]);
    }

    public static int backendFromName(String name) {
        if (name == null) {
            return DXB_BACKEND_AUTO;
        }
        String normalized = name.toLowerCase(Locale.US);
        if ("auto".equals(normalized)) {
            return DXB_BACKEND_AUTO;
        }
        if ("dx11".equals(normalized)) {
            return DXB_BACKEND_DX11;
        }
        if ("dx12".equals(normalized)) {
            return DXB_BACKEND_DX12;
        }
        throw new IllegalArgumentException("Unknown backend: " + name);
    }

    public static String logLevelName(int level) {
        switch (level) {
            case 0:
                return "INFO";
            case 1:
                return "WARN";
            case 2:
                return "ERROR";
            default:
                return "LEVEL_" + level;
        }
    }

    public static String adapterDescription(DXBridgeLibrary.DXBAdapterInfo adapter) {
        return zeroTerminatedUtf8(adapter.description);
    }

    public static DXBridgeLibrary.DXBAdapterInfo[] newAdapterArray(int count) {
        DXBridgeLibrary.DXBAdapterInfo first = new DXBridgeLibrary.DXBAdapterInfo();
        DXBridgeLibrary.DXBAdapterInfo[] array = (DXBridgeLibrary.DXBAdapterInfo[]) first.toArray(count);
        for (int i = 0; i < array.length; i++) {
            array[i].struct_version = DXBRIDGE_STRUCT_VERSION;
            array[i].write();
        }
        return array;
    }

    public static DXBridgeLibrary.DXBInputElementDesc[] newInputElementArray(int count) {
        DXBridgeLibrary.DXBInputElementDesc first = new DXBridgeLibrary.DXBInputElementDesc();
        return (DXBridgeLibrary.DXBInputElementDesc[]) first.toArray(count);
    }

    private static File locateDll(String explicitPath) {
        List<File> candidates = new ArrayList<File>();
        if (explicitPath != null && explicitPath.length() > 0) {
            candidates.add(new File(explicitPath));
        }

        String envPath = System.getenv("DXBRIDGE_DLL");
        if (envPath != null && envPath.length() > 0) {
            candidates.add(new File(envPath));
        }

        File cwd = canonical(new File("."));
        candidates.add(new File(cwd, "dxbridge.dll"));

        File repoRoot = findRepoRoot(cwd);
        if (repoRoot != null) {
            candidates.add(new File(repoRoot, "out/build/debug/Debug/dxbridge.dll"));
            candidates.add(new File(repoRoot, "out/build/debug/examples/Debug/dxbridge.dll"));
            candidates.add(new File(repoRoot, "out/build/debug/tests/Debug/dxbridge.dll"));
            candidates.add(new File(repoRoot, "out/build/ci/Release/dxbridge.dll"));
        }

        for (int i = 0; i < candidates.size(); i++) {
            File candidate = canonical(candidates.get(i));
            if (candidate.isFile()) {
                return candidate;
            }
        }

        StringBuilder message = new StringBuilder();
        message.append("Could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\nSearched:");
        for (int i = 0; i < candidates.size(); i++) {
            message.append("\n  - ").append(canonical(candidates.get(i)).getAbsolutePath());
        }
        throw new IllegalArgumentException(message.toString());
    }

    private static File findRepoRoot(File start) {
        File current = start;
        while (current != null) {
            File marker = new File(current, "include/dxbridge/dxbridge.h");
            if (marker.isFile()) {
                return current;
            }
            current = current.getParentFile();
        }
        return null;
    }

    private static File canonical(File file) {
        try {
            return file.getCanonicalFile();
        } catch (IOException ex) {
            return file.getAbsoluteFile();
        }
    }

    private static String resultName(int result) {
        switch (result) {
            case DXB_OK:
                return "DXB_OK";
            case DXB_E_INVALID_HANDLE:
                return "DXB_E_INVALID_HANDLE";
            case DXB_E_DEVICE_LOST:
                return "DXB_E_DEVICE_LOST";
            case DXB_E_OUT_OF_MEMORY:
                return "DXB_E_OUT_OF_MEMORY";
            case DXB_E_INVALID_ARG:
                return "DXB_E_INVALID_ARG";
            case DXB_E_NOT_SUPPORTED:
                return "DXB_E_NOT_SUPPORTED";
            case DXB_E_SHADER_COMPILE:
                return "DXB_E_SHADER_COMPILE";
            case DXB_E_NOT_INITIALIZED:
                return "DXB_E_NOT_INITIALIZED";
            case DXB_E_INVALID_STATE:
                return "DXB_E_INVALID_STATE";
            case DXB_E_INTERNAL:
                return "DXB_E_INTERNAL";
            default:
                return "DXBResult(" + result + ")";
        }
    }

    private static String pointerToUtf8(Pointer pointer) {
        if (pointer == null) {
            return "";
        }
        return pointer.getString(0, "UTF-8");
    }

    private static String zeroTerminatedUtf8(byte[] data) {
        int end = 0;
        while (end < data.length && data[end] != 0) {
            end++;
        }
        return new String(data, 0, end, UTF8);
    }
}
