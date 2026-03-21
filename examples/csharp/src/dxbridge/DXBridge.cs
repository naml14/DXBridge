using System.Runtime.InteropServices;
using System.Text;

namespace DXBridgeExamples.DxBridge;

internal sealed class DXBridge : IDisposable
{
    public const ulong NullHandle = 0;
    public const uint StructVersion = 1;

    public const int DxbOk = 0;
    public const int DxbEInvalidHandle = -1;
    public const int DxbEDeviceLost = -2;
    public const int DxbEOutOfMemory = -3;
    public const int DxbEInvalidArg = -4;
    public const int DxbENotSupported = -5;
    public const int DxbEShaderCompile = -6;
    public const int DxbENotInitialized = -7;
    public const int DxbEInvalidState = -8;
    public const int DxbEInternal = -999;

    public const uint DxbBackendAuto = 0;
    public const uint DxbBackendDx11 = 1;
    public const uint DxbBackendDx12 = 2;

    public const uint DxbFormatUnknown = 0;
    public const uint DxbFormatRgba8Unorm = 1;
    public const uint DxbFormatD32Float = 3;
    public const uint DxbFormatRg32Float = 6;
    public const uint DxbFormatRgb32Float = 7;

    public const uint DxbDeviceFlagDebug = 0x0001;
    public const uint DxbDeviceFlagGpuValidation = 0x0002;

    public const uint DxbBufferFlagVertex = 0x0001;
    public const uint DxbBufferFlagDynamic = 0x0008;

    public const uint DxbShaderStageVertex = 0;
    public const uint DxbShaderStagePixel = 1;

    public const uint DxbPrimTriangleList = 0;

    public const uint DxbSemanticPosition = 0;
    public const uint DxbSemanticColor = 3;

    public const uint DxbCompileDebug = 0x0001;
    public const uint DxbFeatureDx12 = 0x0001;

    private readonly DXBridgeNative _native;
    private DXBridgeNative.LogCallback? _callback;

    public DXBridge(string? explicitDllPath)
    {
        DllPath = LocateDll(explicitDllPath);
        _native = new DXBridgeNative(DllPath);
    }

    public string DllPath { get; }

    public void Dispose()
    {
        SetLogCallback(null);
        _native.Dispose();
    }

    public void SetLogCallback(Action<int, string, IntPtr>? handler)
    {
        if (handler is null)
        {
            _callback = null;
            _native.SetLogCallback(null, IntPtr.Zero);
            return;
        }

        _callback = (level, message, userData) =>
        {
            handler(level, message == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(message) ?? string.Empty, userData);
        };
        _native.SetLogCallback(_callback, IntPtr.Zero);
    }

    public uint GetVersion() => _native.GetVersion();

    public void Init(uint backend)
    {
        RequireOk(_native.Init(backend), "DXBridge_Init");
    }

    public void Shutdown()
    {
        _native.Shutdown();
    }

    public bool SupportsFeature(uint featureFlag) => _native.SupportsFeature(featureFlag) != 0;

    public string GetLastError()
    {
        var buffer = new byte[512];
        _native.GetLastError(buffer, buffer.Length);
        var length = Array.IndexOf(buffer, (byte)0);
        if (length < 0)
        {
            length = buffer.Length;
        }

        return Encoding.UTF8.GetString(buffer, 0, length);
    }

    public uint GetLastHResult() => _native.GetLastHResult();

    public string DescribeFailure(int result)
    {
        var message = GetLastError();
        var name = ResultName(result);
        var hresult = FormatHResult(GetLastHResult());
        return string.IsNullOrEmpty(message) ? $"{name} (HRESULT {hresult})" : $"{name}: {message} (HRESULT {hresult})";
    }

    public void RequireOk(int result, string functionName)
    {
        if (result != DxbOk)
        {
            throw new DXBridgeException($"{functionName} failed: {DescribeFailure(result)}");
        }
    }

    public int CreateDeviceRaw(IntPtr desc, out ulong device)
    {
        return _native.CreateDevice(desc, out device);
    }

    public IReadOnlyList<DXBAdapterInfo> EnumerateAdapters()
    {
        uint count = 0;
        RequireOk(_native.EnumerateAdapters(IntPtr.Zero, ref count), "DXBridge_EnumerateAdapters(count)");
        if (count == 0)
        {
            return Array.Empty<DXBAdapterInfo>();
        }

        using var array = new UnmanagedArray<DXBAdapterInfo>((int)count, static _ => new DXBAdapterInfo
        {
            StructVersion = StructVersion,
            Description = new byte[128],
        });

        RequireOk(_native.EnumerateAdapters(array.Pointer, ref count), "DXBridge_EnumerateAdapters(fill)");
        return array.Read(count);
    }

    public ulong CreateDevice(uint backend, uint adapterIndex = 0, uint flags = 0)
    {
        var desc = new DXBDeviceDesc
        {
            StructVersion = StructVersion,
            Backend = backend,
            AdapterIndex = adapterIndex,
            Flags = flags,
        };

        using var nativeDesc = new UnmanagedStruct<DXBDeviceDesc>(desc);
        RequireOk(_native.CreateDevice(nativeDesc.Pointer, out var device), "DXBridge_CreateDevice");
        return device;
    }

    public void DestroyDevice(ulong device) => _native.DestroyDevice(device);

    public uint GetFeatureLevel(ulong device) => _native.GetFeatureLevel(device);

    public ulong CreateSwapChain(ulong device, IntPtr hwnd, int width, int height, uint format = DxbFormatRgba8Unorm, uint bufferCount = 2, uint flags = 0)
    {
        var desc = new DXBSwapChainDesc
        {
            StructVersion = StructVersion,
            Hwnd = hwnd,
            Width = checked((uint)width),
            Height = checked((uint)height),
            Format = format,
            BufferCount = bufferCount,
            Flags = flags,
        };

        using var nativeDesc = new UnmanagedStruct<DXBSwapChainDesc>(desc);
        RequireOk(_native.CreateSwapChain(device, nativeDesc.Pointer, out var swapChain), "DXBridge_CreateSwapChain");
        return swapChain;
    }

    public void DestroySwapChain(ulong swapChain) => _native.DestroySwapChain(swapChain);

    public void ResizeSwapChain(ulong swapChain, int width, int height)
    {
        RequireOk(_native.ResizeSwapChain(swapChain, checked((uint)width), checked((uint)height)), "DXBridge_ResizeSwapChain");
    }

    public ulong GetBackBuffer(ulong swapChain)
    {
        RequireOk(_native.GetBackBuffer(swapChain, out var renderTarget), "DXBridge_GetBackBuffer");
        return renderTarget;
    }

    public ulong CreateRenderTargetView(ulong device, ulong renderTarget)
    {
        RequireOk(_native.CreateRenderTargetView(device, renderTarget, out var rtv), "DXBridge_CreateRenderTargetView");
        return rtv;
    }

    public void DestroyRenderTargetView(ulong rtv) => _native.DestroyRtv(rtv);

    public ulong CompileShader(ulong device, uint stage, string sourceCode, string sourceName, string entryPoint, string target, uint compileFlags = 0)
    {
        using var utf8 = new Utf8Strings(sourceCode, sourceName, entryPoint, target);
        var desc = new DXBShaderDesc
        {
            StructVersion = StructVersion,
            Stage = stage,
            SourceCode = utf8.SourceCode,
            SourceSize = 0,
            SourceName = utf8.SourceName,
            EntryPoint = utf8.EntryPoint,
            Target = utf8.Target,
            CompileFlags = compileFlags,
        };

        using var nativeDesc = new UnmanagedStruct<DXBShaderDesc>(desc);
        RequireOk(_native.CompileShader(device, nativeDesc.Pointer, out var shader), "DXBridge_CompileShader");
        return shader;
    }

    public void DestroyShader(ulong shader) => _native.DestroyShader(shader);

    public ulong CreateInputLayout(ulong device, IReadOnlyList<DXBInputElementDesc> elements, ulong vertexShader)
    {
        if (elements.Count == 0)
        {
            throw new ArgumentException("elements must not be empty");
        }

        using var nativeElements = new UnmanagedArray<DXBInputElementDesc>(elements);
        RequireOk(_native.CreateInputLayout(device, nativeElements.Pointer, checked((uint)elements.Count), vertexShader, out var inputLayout), "DXBridge_CreateInputLayout");
        return inputLayout;
    }

    public void DestroyInputLayout(ulong inputLayout) => _native.DestroyInputLayout(inputLayout);

    public ulong CreatePipelineState(ulong device, ulong vertexShader, ulong pixelShader, ulong inputLayout, uint topology = DxbPrimTriangleList, uint depthTest = 0, uint depthWrite = 0, uint cullBack = 0, uint wireframe = 0)
    {
        var desc = new DXBPipelineDesc
        {
            StructVersion = StructVersion,
            Vs = vertexShader,
            Ps = pixelShader,
            InputLayout = inputLayout,
            Topology = topology,
            DepthTest = depthTest,
            DepthWrite = depthWrite,
            CullBack = cullBack,
            Wireframe = wireframe,
        };

        using var nativeDesc = new UnmanagedStruct<DXBPipelineDesc>(desc);
        RequireOk(_native.CreatePipelineState(device, nativeDesc.Pointer, out var pipeline), "DXBridge_CreatePipelineState");
        return pipeline;
    }

    public void DestroyPipeline(ulong pipeline) => _native.DestroyPipeline(pipeline);

    public ulong CreateBuffer(ulong device, uint flags, int byteSize, int stride)
    {
        var desc = new DXBBufferDesc
        {
            StructVersion = StructVersion,
            Flags = flags,
            ByteSize = checked((uint)byteSize),
            Stride = checked((uint)stride),
        };

        using var nativeDesc = new UnmanagedStruct<DXBBufferDesc>(desc);
        RequireOk(_native.CreateBuffer(device, nativeDesc.Pointer, out var buffer), "DXBridge_CreateBuffer");
        return buffer;
    }

    public void UploadFloats(ulong buffer, float[] data)
    {
        var handle = GCHandle.Alloc(data, GCHandleType.Pinned);
        try
        {
            var byteSize = checked((uint)(data.Length * sizeof(float)));
            RequireOk(_native.UploadData(buffer, handle.AddrOfPinnedObject(), byteSize), "DXBridge_UploadData");
        }
        finally
        {
            handle.Free();
        }
    }

    public void DestroyBuffer(ulong buffer) => _native.DestroyBuffer(buffer);

    public void BeginFrame(ulong device) => RequireOk(_native.BeginFrame(device), "DXBridge_BeginFrame");

    public void SetRenderTargets(ulong device, ulong rtv, ulong dsv = NullHandle)
    {
        RequireOk(_native.SetRenderTargets(device, rtv, dsv), "DXBridge_SetRenderTargets");
    }

    public void ClearRenderTarget(ulong rtv, float[] rgba)
    {
        if (rgba.Length != 4)
        {
            throw new ArgumentException("rgba must contain exactly 4 values");
        }

        RequireOk(_native.ClearRenderTarget(rtv, rgba), "DXBridge_ClearRenderTarget");
    }

    public void SetViewport(ulong device, float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
    {
        var viewport = new DXBViewport
        {
            X = x,
            Y = y,
            Width = width,
            Height = height,
            MinDepth = minDepth,
            MaxDepth = maxDepth,
        };

        using var nativeViewport = new UnmanagedStruct<DXBViewport>(viewport);
        RequireOk(_native.SetViewport(device, nativeViewport.Pointer), "DXBridge_SetViewport");
    }

    public void SetPipeline(ulong device, ulong pipeline)
    {
        RequireOk(_native.SetPipeline(device, pipeline), "DXBridge_SetPipeline");
    }

    public void SetVertexBuffer(ulong device, ulong buffer, int stride, int offset = 0)
    {
        RequireOk(_native.SetVertexBuffer(device, buffer, checked((uint)stride), checked((uint)offset)), "DXBridge_SetVertexBuffer");
    }

    public void Draw(ulong device, int vertexCount, int startVertex = 0)
    {
        RequireOk(_native.Draw(device, checked((uint)vertexCount), checked((uint)startVertex)), "DXBridge_Draw");
    }

    public void EndFrame(ulong device) => RequireOk(_native.EndFrame(device), "DXBridge_EndFrame");

    public void Present(ulong swapChain, int syncInterval = 1)
    {
        RequireOk(_native.Present(swapChain, checked((uint)Math.Max(0, syncInterval))), "DXBridge_Present");
    }

    public static string ParseVersion(uint version)
    {
        var major = (version >> 16) & 0xFF;
        var minor = (version >> 8) & 0xFF;
        var patch = version & 0xFF;
        return $"{major}.{minor}.{patch}";
    }

    public static string FormatHResult(uint value) => $"0x{value:X8}";

    public static string FormatBytes(ulong value)
    {
        var units = new[] { "B", "KiB", "MiB", "GiB", "TiB" };
        var amount = (double)value;
        var index = 0;
        while (amount >= 1024.0 && index < units.Length - 1)
        {
            amount /= 1024.0;
            index++;
        }

        return index == 0 ? $"{value} {units[index]}" : $"{amount:0.0} {units[index]}";
    }

    public static uint BackendFromName(string? name)
    {
        return (name ?? "auto").ToLowerInvariant() switch
        {
            "auto" => DxbBackendAuto,
            "dx11" => DxbBackendDx11,
            "dx12" => DxbBackendDx12,
            _ => throw new ArgumentException($"Unknown backend: {name}"),
        };
    }

    public static string LogLevelName(int level)
    {
        return level switch
        {
            0 => "INFO",
            1 => "WARN",
            2 => "ERROR",
            _ => $"LEVEL_{level}",
        };
    }

    public static string AdapterDescription(DXBAdapterInfo adapter)
    {
        var description = adapter.Description ?? Array.Empty<byte>();
        var length = Array.IndexOf(description, (byte)0);
        if (length < 0)
        {
            length = description.Length;
        }

        return Encoding.UTF8.GetString(description, 0, length);
    }

    private static string LocateDll(string? explicitPath)
    {
        var candidates = new List<string>();

        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            candidates.Add(explicitPath);
        }

        var envPath = Environment.GetEnvironmentVariable("DXBRIDGE_DLL");
        if (!string.IsNullOrWhiteSpace(envPath))
        {
            candidates.Add(envPath);
        }

        candidates.Add(Path.Combine(Directory.GetCurrentDirectory(), "dxbridge.dll"));
        candidates.Add(Path.Combine(AppContext.BaseDirectory, "dxbridge.dll"));

        var repoRoot = FindRepoRoot(Directory.GetCurrentDirectory()) ?? FindRepoRoot(AppContext.BaseDirectory);
        if (repoRoot is not null)
        {
            candidates.Add(Path.Combine(repoRoot, "build", "debug", "Debug", "dxbridge.dll"));
            candidates.Add(Path.Combine(repoRoot, "build", "debug", "examples", "Debug", "dxbridge.dll"));
            candidates.Add(Path.Combine(repoRoot, "build", "debug", "tests", "Debug", "dxbridge.dll"));
            candidates.Add(Path.Combine(repoRoot, "out", "build", "ci", "Debug", "dxbridge.dll"));
        }

        foreach (var candidate in candidates)
        {
            var fullPath = Path.GetFullPath(candidate);
            if (File.Exists(fullPath))
            {
                return fullPath;
            }
        }

        var message = new StringBuilder();
        message.AppendLine("Could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.");
        message.AppendLine("Searched:");
        foreach (var candidate in candidates)
        {
            message.Append("  - ").AppendLine(Path.GetFullPath(candidate));
        }

        throw new FileNotFoundException(message.ToString());
    }

    private static string? FindRepoRoot(string startPath)
    {
        var current = new DirectoryInfo(Path.GetFullPath(startPath));
        while (current is not null)
        {
            var marker = Path.Combine(current.FullName, "include", "dxbridge", "dxbridge.h");
            if (File.Exists(marker))
            {
                return current.FullName;
            }

            current = current.Parent;
        }

        return null;
    }

    private static string ResultName(int result)
    {
        return result switch
        {
            DxbOk => "DXB_OK",
            DxbEInvalidHandle => "DXB_E_INVALID_HANDLE",
            DxbEDeviceLost => "DXB_E_DEVICE_LOST",
            DxbEOutOfMemory => "DXB_E_OUT_OF_MEMORY",
            DxbEInvalidArg => "DXB_E_INVALID_ARG",
            DxbENotSupported => "DXB_E_NOT_SUPPORTED",
            DxbEShaderCompile => "DXB_E_SHADER_COMPILE",
            DxbENotInitialized => "DXB_E_NOT_INITIALIZED",
            DxbEInvalidState => "DXB_E_INVALID_STATE",
            DxbEInternal => "DXB_E_INTERNAL",
            _ => $"DXBResult({result})",
        };
    }

    private sealed class UnmanagedStruct<T> : IDisposable where T : struct
    {
        public UnmanagedStruct(T value)
        {
            Pointer = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
            Marshal.StructureToPtr(value, Pointer, false);
        }

        public IntPtr Pointer { get; }

        public void Dispose()
        {
            Marshal.FreeHGlobal(Pointer);
        }
    }

    private sealed class UnmanagedArray<T> : IDisposable where T : struct
    {
        private readonly int _size = Marshal.SizeOf<T>();
        private readonly int _count;

        public UnmanagedArray(int count, Func<int, T> factory)
        {
            _count = count;
            Pointer = Marshal.AllocHGlobal(_size * count);
            for (var i = 0; i < count; i++)
            {
                Marshal.StructureToPtr(factory(i), IntPtr.Add(Pointer, i * _size), false);
            }
        }

        public UnmanagedArray(IReadOnlyList<T> values)
        {
            _count = values.Count;
            Pointer = Marshal.AllocHGlobal(_size * _count);
            for (var i = 0; i < _count; i++)
            {
                Marshal.StructureToPtr(values[i], IntPtr.Add(Pointer, i * _size), false);
            }
        }

        public IntPtr Pointer { get; }

        public List<T> Read(uint count)
        {
            var result = new List<T>((int)count);
            for (var i = 0; i < count; i++)
            {
                result.Add(Marshal.PtrToStructure<T>(IntPtr.Add(Pointer, (int)i * _size)));
            }

            return result;
        }

        public void Dispose()
        {
            Marshal.FreeHGlobal(Pointer);
        }
    }

    private sealed class Utf8Strings : IDisposable
    {
        public Utf8Strings(string sourceCode, string sourceName, string entryPoint, string target)
        {
            SourceCode = Marshal.StringToCoTaskMemUTF8(sourceCode);
            SourceName = Marshal.StringToCoTaskMemUTF8(sourceName);
            EntryPoint = Marshal.StringToCoTaskMemUTF8(entryPoint);
            Target = Marshal.StringToCoTaskMemUTF8(target);
        }

        public IntPtr SourceCode { get; }
        public IntPtr SourceName { get; }
        public IntPtr EntryPoint { get; }
        public IntPtr Target { get; }

        public void Dispose()
        {
            Marshal.FreeCoTaskMem(SourceCode);
            Marshal.FreeCoTaskMem(SourceName);
            Marshal.FreeCoTaskMem(EntryPoint);
            Marshal.FreeCoTaskMem(Target);
        }
    }
}
