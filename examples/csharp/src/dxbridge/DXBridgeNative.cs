using System.Runtime.InteropServices;

namespace DXBridgeExamples.DxBridge;

[StructLayout(LayoutKind.Sequential)]
internal struct DXBAdapterInfo
{
    public uint StructVersion;
    public uint Index;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
    public byte[] Description;

    public ulong DedicatedVideoMem;
    public ulong DedicatedSystemMem;
    public ulong SharedSystemMem;
    public uint IsSoftware;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBDeviceDesc
{
    public uint StructVersion;
    public uint Backend;
    public uint AdapterIndex;
    public uint Flags;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBSwapChainDesc
{
    public uint StructVersion;
    public IntPtr Hwnd;
    public uint Width;
    public uint Height;
    public uint Format;
    public uint BufferCount;
    public uint Flags;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBBufferDesc
{
    public uint StructVersion;
    public uint Flags;
    public uint ByteSize;
    public uint Stride;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBShaderDesc
{
    public uint StructVersion;
    public uint Stage;
    public IntPtr SourceCode;
    public uint SourceSize;
    public IntPtr SourceName;
    public IntPtr EntryPoint;
    public IntPtr Target;
    public uint CompileFlags;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBInputElementDesc
{
    public uint StructVersion;
    public uint Semantic;
    public uint SemanticIndex;
    public uint Format;
    public uint InputSlot;
    public uint ByteOffset;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBPipelineDesc
{
    public uint StructVersion;
    public ulong Vs;
    public ulong Ps;
    public ulong InputLayout;
    public uint Topology;
    public uint DepthTest;
    public uint DepthWrite;
    public uint CullBack;
    public uint Wireframe;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DXBViewport
{
    public float X;
    public float Y;
    public float Width;
    public float Height;
    public float MinDepth;
    public float MaxDepth;
}

internal sealed class DXBridgeNative : IDisposable
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void LogCallback(int level, IntPtr message, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void SetLogCallbackDelegate(LogCallback? callback, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InitDelegate(uint backendHint);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void ShutdownDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint GetVersionDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint SupportsFeatureDelegate(uint featureFlag);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void GetLastErrorDelegate(byte[] buffer, int bufferSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint GetLastHResultDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int EnumerateAdaptersDelegate(IntPtr outBuffer, ref uint inOutCount);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreateDeviceDelegate(IntPtr desc, out ulong outDevice);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyDeviceDelegate(ulong device);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint GetFeatureLevelDelegate(ulong device);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreateSwapChainDelegate(ulong device, IntPtr desc, out ulong outSwapChain);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroySwapChainDelegate(ulong swapChain);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int ResizeSwapChainDelegate(ulong swapChain, uint width, uint height);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int GetBackBufferDelegate(ulong swapChain, out ulong outRenderTarget);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreateRenderTargetViewDelegate(ulong device, ulong renderTarget, out ulong outRtv);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyRtvDelegate(ulong rtv);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CompileShaderDelegate(ulong device, IntPtr desc, out ulong outShader);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyShaderDelegate(ulong shader);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreateInputLayoutDelegate(ulong device, IntPtr elements, uint elementCount, ulong vertexShader, out ulong outLayout);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyInputLayoutDelegate(ulong layout);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreatePipelineStateDelegate(ulong device, IntPtr desc, out ulong outPipeline);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyPipelineDelegate(ulong pipeline);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CreateBufferDelegate(ulong device, IntPtr desc, out ulong outBuffer);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int UploadDataDelegate(ulong buffer, IntPtr data, uint byteSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DestroyBufferDelegate(ulong buffer);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int BeginFrameDelegate(ulong device);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SetRenderTargetsDelegate(ulong device, ulong rtv, ulong dsv);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int ClearRenderTargetDelegate(ulong rtv, float[] rgba);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SetViewportDelegate(ulong device, IntPtr viewport);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SetPipelineDelegate(ulong device, ulong pipeline);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int SetVertexBufferDelegate(ulong device, ulong buffer, uint stride, uint offset);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int DrawDelegate(ulong device, uint vertexCount, uint startVertex);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int EndFrameDelegate(ulong device);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int PresentDelegate(ulong swapChain, uint syncInterval);

    private readonly nint _handle;
    private readonly SetLogCallbackDelegate _setLogCallback;
    private readonly InitDelegate _init;
    private readonly ShutdownDelegate _shutdown;
    private readonly GetVersionDelegate _getVersion;
    private readonly SupportsFeatureDelegate _supportsFeature;
    private readonly GetLastErrorDelegate _getLastError;
    private readonly GetLastHResultDelegate _getLastHResult;
    private readonly EnumerateAdaptersDelegate _enumerateAdapters;
    private readonly CreateDeviceDelegate _createDevice;
    private readonly DestroyDeviceDelegate _destroyDevice;
    private readonly GetFeatureLevelDelegate _getFeatureLevel;
    private readonly CreateSwapChainDelegate _createSwapChain;
    private readonly DestroySwapChainDelegate _destroySwapChain;
    private readonly ResizeSwapChainDelegate _resizeSwapChain;
    private readonly GetBackBufferDelegate _getBackBuffer;
    private readonly CreateRenderTargetViewDelegate _createRenderTargetView;
    private readonly DestroyRtvDelegate _destroyRtv;
    private readonly CompileShaderDelegate _compileShader;
    private readonly DestroyShaderDelegate _destroyShader;
    private readonly CreateInputLayoutDelegate _createInputLayout;
    private readonly DestroyInputLayoutDelegate _destroyInputLayout;
    private readonly CreatePipelineStateDelegate _createPipelineState;
    private readonly DestroyPipelineDelegate _destroyPipeline;
    private readonly CreateBufferDelegate _createBuffer;
    private readonly UploadDataDelegate _uploadData;
    private readonly DestroyBufferDelegate _destroyBuffer;
    private readonly BeginFrameDelegate _beginFrame;
    private readonly SetRenderTargetsDelegate _setRenderTargets;
    private readonly ClearRenderTargetDelegate _clearRenderTarget;
    private readonly SetViewportDelegate _setViewport;
    private readonly SetPipelineDelegate _setPipeline;
    private readonly SetVertexBufferDelegate _setVertexBuffer;
    private readonly DrawDelegate _draw;
    private readonly EndFrameDelegate _endFrame;
    private readonly PresentDelegate _present;

    public DXBridgeNative(string dllPath)
    {
        _handle = NativeLibrary.Load(dllPath);
        _setLogCallback = Load<SetLogCallbackDelegate>("DXBridge_SetLogCallback");
        _init = Load<InitDelegate>("DXBridge_Init");
        _shutdown = Load<ShutdownDelegate>("DXBridge_Shutdown");
        _getVersion = Load<GetVersionDelegate>("DXBridge_GetVersion");
        _supportsFeature = Load<SupportsFeatureDelegate>("DXBridge_SupportsFeature");
        _getLastError = Load<GetLastErrorDelegate>("DXBridge_GetLastError");
        _getLastHResult = Load<GetLastHResultDelegate>("DXBridge_GetLastHRESULT");
        _enumerateAdapters = Load<EnumerateAdaptersDelegate>("DXBridge_EnumerateAdapters");
        _createDevice = Load<CreateDeviceDelegate>("DXBridge_CreateDevice");
        _destroyDevice = Load<DestroyDeviceDelegate>("DXBridge_DestroyDevice");
        _getFeatureLevel = Load<GetFeatureLevelDelegate>("DXBridge_GetFeatureLevel");
        _createSwapChain = Load<CreateSwapChainDelegate>("DXBridge_CreateSwapChain");
        _destroySwapChain = Load<DestroySwapChainDelegate>("DXBridge_DestroySwapChain");
        _resizeSwapChain = Load<ResizeSwapChainDelegate>("DXBridge_ResizeSwapChain");
        _getBackBuffer = Load<GetBackBufferDelegate>("DXBridge_GetBackBuffer");
        _createRenderTargetView = Load<CreateRenderTargetViewDelegate>("DXBridge_CreateRenderTargetView");
        _destroyRtv = Load<DestroyRtvDelegate>("DXBridge_DestroyRTV");
        _compileShader = Load<CompileShaderDelegate>("DXBridge_CompileShader");
        _destroyShader = Load<DestroyShaderDelegate>("DXBridge_DestroyShader");
        _createInputLayout = Load<CreateInputLayoutDelegate>("DXBridge_CreateInputLayout");
        _destroyInputLayout = Load<DestroyInputLayoutDelegate>("DXBridge_DestroyInputLayout");
        _createPipelineState = Load<CreatePipelineStateDelegate>("DXBridge_CreatePipelineState");
        _destroyPipeline = Load<DestroyPipelineDelegate>("DXBridge_DestroyPipeline");
        _createBuffer = Load<CreateBufferDelegate>("DXBridge_CreateBuffer");
        _uploadData = Load<UploadDataDelegate>("DXBridge_UploadData");
        _destroyBuffer = Load<DestroyBufferDelegate>("DXBridge_DestroyBuffer");
        _beginFrame = Load<BeginFrameDelegate>("DXBridge_BeginFrame");
        _setRenderTargets = Load<SetRenderTargetsDelegate>("DXBridge_SetRenderTargets");
        _clearRenderTarget = Load<ClearRenderTargetDelegate>("DXBridge_ClearRenderTarget");
        _setViewport = Load<SetViewportDelegate>("DXBridge_SetViewport");
        _setPipeline = Load<SetPipelineDelegate>("DXBridge_SetPipeline");
        _setVertexBuffer = Load<SetVertexBufferDelegate>("DXBridge_SetVertexBuffer");
        _draw = Load<DrawDelegate>("DXBridge_Draw");
        _endFrame = Load<EndFrameDelegate>("DXBridge_EndFrame");
        _present = Load<PresentDelegate>("DXBridge_Present");
    }

    public void SetLogCallback(LogCallback? callback, IntPtr userData) => _setLogCallback(callback, userData);
    public int Init(uint backendHint) => _init(backendHint);
    public void Shutdown() => _shutdown();
    public uint GetVersion() => _getVersion();
    public uint SupportsFeature(uint featureFlag) => _supportsFeature(featureFlag);
    public void GetLastError(byte[] buffer, int bufferSize) => _getLastError(buffer, bufferSize);
    public uint GetLastHResult() => _getLastHResult();
    public int EnumerateAdapters(IntPtr outBuffer, ref uint inOutCount) => _enumerateAdapters(outBuffer, ref inOutCount);
    public int CreateDevice(IntPtr desc, out ulong outDevice) => _createDevice(desc, out outDevice);
    public void DestroyDevice(ulong device) => _destroyDevice(device);
    public uint GetFeatureLevel(ulong device) => _getFeatureLevel(device);
    public int CreateSwapChain(ulong device, IntPtr desc, out ulong outSwapChain) => _createSwapChain(device, desc, out outSwapChain);
    public void DestroySwapChain(ulong swapChain) => _destroySwapChain(swapChain);
    public int ResizeSwapChain(ulong swapChain, uint width, uint height) => _resizeSwapChain(swapChain, width, height);
    public int GetBackBuffer(ulong swapChain, out ulong outRenderTarget) => _getBackBuffer(swapChain, out outRenderTarget);
    public int CreateRenderTargetView(ulong device, ulong renderTarget, out ulong outRtv) => _createRenderTargetView(device, renderTarget, out outRtv);
    public void DestroyRtv(ulong rtv) => _destroyRtv(rtv);
    public int CompileShader(ulong device, IntPtr desc, out ulong outShader) => _compileShader(device, desc, out outShader);
    public void DestroyShader(ulong shader) => _destroyShader(shader);
    public int CreateInputLayout(ulong device, IntPtr elements, uint elementCount, ulong vertexShader, out ulong outLayout) => _createInputLayout(device, elements, elementCount, vertexShader, out outLayout);
    public void DestroyInputLayout(ulong layout) => _destroyInputLayout(layout);
    public int CreatePipelineState(ulong device, IntPtr desc, out ulong outPipeline) => _createPipelineState(device, desc, out outPipeline);
    public void DestroyPipeline(ulong pipeline) => _destroyPipeline(pipeline);
    public int CreateBuffer(ulong device, IntPtr desc, out ulong outBuffer) => _createBuffer(device, desc, out outBuffer);
    public int UploadData(ulong buffer, IntPtr data, uint byteSize) => _uploadData(buffer, data, byteSize);
    public void DestroyBuffer(ulong buffer) => _destroyBuffer(buffer);
    public int BeginFrame(ulong device) => _beginFrame(device);
    public int SetRenderTargets(ulong device, ulong rtv, ulong dsv) => _setRenderTargets(device, rtv, dsv);
    public int ClearRenderTarget(ulong rtv, float[] rgba) => _clearRenderTarget(rtv, rgba);
    public int SetViewport(ulong device, IntPtr viewport) => _setViewport(device, viewport);
    public int SetPipeline(ulong device, ulong pipeline) => _setPipeline(device, pipeline);
    public int SetVertexBuffer(ulong device, ulong buffer, uint stride, uint offset) => _setVertexBuffer(device, buffer, stride, offset);
    public int Draw(ulong device, uint vertexCount, uint startVertex) => _draw(device, vertexCount, startVertex);
    public int EndFrame(ulong device) => _endFrame(device);
    public int Present(ulong swapChain, uint syncInterval) => _present(swapChain, syncInterval);

    public void Dispose()
    {
        NativeLibrary.Free(_handle);
    }

    private T Load<T>(string exportName) where T : Delegate
    {
        var address = NativeLibrary.GetExport(_handle, exportName);
        return Marshal.GetDelegateForFunctionPointer<T>(address);
    }
}
