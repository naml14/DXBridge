using DXBridgeExamples.DxBridge;

namespace DXBridgeExamples.Examples;

internal static class Example05Dx11MovingTriangle
{
    private const string VsSource =
        "struct VSInput { float2 pos : POSITION; float3 col : COLOR; };\n"
        + "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
        + "PSInput main(VSInput v) {\n"
        + "    PSInput o;\n"
        + "    o.pos = float4(v.pos, 0.0f, 1.0f);\n"
        + "    o.col = v.col;\n"
        + "    return o;\n"
        + "}\n";

    private const string PsSource =
        "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
        + "float4 main(PSInput p) : SV_TARGET {\n"
        + "    return float4(p.col, 1.0f);\n"
        + "}\n";

    public static int Run(string[] args)
    {
        var cli = new CommandLine(
            args,
            new[] { "--dll", "--width", "--height", "--frames", "--speed", "--amplitude", "--sync-interval" },
            new[] { "--hidden", "--debug" });
        if (cli.HelpRequested)
        {
            Console.WriteLine("Usage: Example05Dx11MovingTriangle [--dll PATH] [--width 960] [--height 540] [--frames 0] [--speed 1.6] [--amplitude 0.38] [--hidden] [--debug] [--sync-interval 1]");
            return 0;
        }

        var width = cli.GetInt("--width", 960);
        var height = cli.GetInt("--height", 540);
        var frames = cli.GetInt("--frames", 0);
        var speed = cli.GetFloat("--speed", 1.6f);
        var amplitude = cli.GetFloat("--amplitude", 0.38f);
        var syncInterval = cli.GetInt("--sync-interval", 1);

        using var window = new Win32Window("DXBridge C# Moving Triangle", width, height, !cli.HasFlag("--hidden"), "DXBridgeCSharpMovingTriangle");
        using var bridge = new DXBridge(cli.GetString("--dll"));

        var device = DXBridge.NullHandle;
        var swapChain = DXBridge.NullHandle;
        var rtv = DXBridge.NullHandle;
        var vertexShader = DXBridge.NullHandle;
        var pixelShader = DXBridge.NullHandle;
        var inputLayout = DXBridge.NullHandle;
        var pipeline = DXBridge.NullHandle;
        var vertexBuffer = DXBridge.NullHandle;
        var initialized = false;

        try
        {
            window.Create();
            Console.WriteLine($"Loaded: {bridge.DllPath}");
            Console.WriteLine($"Created HWND: 0x{window.Handle.ToInt64():X16}");

            bridge.Init(DXBridge.DxbBackendDx11);
            initialized = true;

            device = bridge.CreateDevice(DXBridge.DxbBackendDx11, 0, cli.HasFlag("--debug") ? DXBridge.DxbDeviceFlagDebug : 0u);
            swapChain = bridge.CreateSwapChain(device, window.Handle, width, height);
            var backBuffer = bridge.GetBackBuffer(swapChain);
            rtv = bridge.CreateRenderTargetView(device, backBuffer);

            vertexShader = bridge.CompileShader(device, DXBridge.DxbShaderStageVertex, VsSource, "moving_triangle_vs", "main", "vs_5_0", DXBridge.DxbCompileDebug);
            pixelShader = bridge.CompileShader(device, DXBridge.DxbShaderStagePixel, PsSource, "moving_triangle_ps", "main", "ps_5_0", DXBridge.DxbCompileDebug);

            var elements = new[]
            {
                new DXBInputElementDesc
                {
                    StructVersion = DXBridge.StructVersion,
                    Semantic = DXBridge.DxbSemanticPosition,
                    SemanticIndex = 0,
                    Format = DXBridge.DxbFormatRg32Float,
                    InputSlot = 0,
                    ByteOffset = 0,
                },
                new DXBInputElementDesc
                {
                    StructVersion = DXBridge.StructVersion,
                    Semantic = DXBridge.DxbSemanticColor,
                    SemanticIndex = 0,
                    Format = DXBridge.DxbFormatRgb32Float,
                    InputSlot = 0,
                    ByteOffset = 8,
                },
            };

            inputLayout = bridge.CreateInputLayout(device, elements, vertexShader);
            pipeline = bridge.CreatePipelineState(device, vertexShader, pixelShader, inputLayout);
            vertexBuffer = bridge.CreateBuffer(device, DXBridge.DxbBufferFlagVertex | DXBridge.DxbBufferFlagDynamic, 15 * sizeof(float), 5 * sizeof(float));

            var startTime = DateTime.UtcNow;
            var frameIndex = 0;
            Console.WriteLine("Rendering triangle animation. Close the window to exit.");

            while (window.PumpMessages())
            {
                var elapsed = (DateTime.UtcNow - startTime).TotalSeconds;
                var offsetX = (float)(amplitude * Math.Sin(elapsed * speed));
                var offsetY = (float)(0.06 * Math.Sin(elapsed * speed * 1.9));

                bridge.UploadFloats(vertexBuffer, MakeVertexData(offsetX, offsetY));

                bridge.BeginFrame(device);
                bridge.SetRenderTargets(device, rtv);
                bridge.ClearRenderTarget(rtv, new[] { 0.04f, 0.05f, 0.08f, 1.0f });
                bridge.SetViewport(device, 0.0f, 0.0f, width, height, 0.0f, 1.0f);
                bridge.SetPipeline(device, pipeline);
                bridge.SetVertexBuffer(device, vertexBuffer, 5 * sizeof(float), 0);
                bridge.Draw(device, 3, 0);
                bridge.EndFrame(device);
                bridge.Present(swapChain, syncInterval);

                frameIndex++;
                if (frames > 0 && frameIndex >= frames)
                {
                    window.Destroy();
                    break;
                }
            }

            Console.WriteLine($"Done. Rendered frames: {frameIndex}");
        }
        finally
        {
            if (vertexBuffer != DXBridge.NullHandle)
            {
                bridge.DestroyBuffer(vertexBuffer);
            }

            if (pipeline != DXBridge.NullHandle)
            {
                bridge.DestroyPipeline(pipeline);
            }

            if (inputLayout != DXBridge.NullHandle)
            {
                bridge.DestroyInputLayout(inputLayout);
            }

            if (pixelShader != DXBridge.NullHandle)
            {
                bridge.DestroyShader(pixelShader);
            }

            if (vertexShader != DXBridge.NullHandle)
            {
                bridge.DestroyShader(vertexShader);
            }

            if (rtv != DXBridge.NullHandle)
            {
                bridge.DestroyRenderTargetView(rtv);
            }

            if (swapChain != DXBridge.NullHandle)
            {
                bridge.DestroySwapChain(swapChain);
            }

            if (device != DXBridge.NullHandle)
            {
                bridge.DestroyDevice(device);
            }

            if (initialized)
            {
                bridge.Shutdown();
            }
        }

        return 0;
    }

    private static float[] MakeVertexData(float offsetX, float offsetY)
    {
        return
        [
            0.0f + offsetX, 0.52f + offsetY, 1.0f, 0.1f, 0.1f,
            -0.42f + offsetX, -0.38f + offsetY, 0.1f, 1.0f, 0.1f,
            0.42f + offsetX, -0.38f + offsetY, 0.1f, 0.3f, 1.0f,
        ];
    }
}
