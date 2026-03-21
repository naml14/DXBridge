using DXBridgeExamples.DxBridge;

namespace DXBridgeExamples.Examples;

internal static class Example04Dx11ClearWindow
{
    public static int Run(string[] args)
    {
        var cli = new CommandLine(
            args,
            new[] { "--dll", "--width", "--height", "--frames", "--color" },
            new[] { "--hidden", "--debug" });
        if (cli.HelpRequested)
        {
            Console.WriteLine("Usage: Example04Dx11ClearWindow [--dll PATH] [--width 640] [--height 360] [--frames 180] [--color R,G,B,A] [--hidden] [--debug]");
            return 0;
        }

        var width = cli.GetInt("--width", 640);
        var height = cli.GetInt("--height", 360);
        var frames = cli.GetInt("--frames", 180);
        var color = cli.GetFloatList("--color", 4, new[] { 0.08f, 0.16f, 0.32f, 1.0f });

        using var window = new Win32Window("DXBridge C# Clear Window", width, height, !cli.HasFlag("--hidden"), "DXBridgeCSharpClearWindow");
        using var bridge = new DXBridge(cli.GetString("--dll"));

        var device = DXBridge.NullHandle;
        var swapChain = DXBridge.NullHandle;
        var rtv = DXBridge.NullHandle;
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

            Console.WriteLine("Rendering frames. Close the window manually or wait for auto-close.");
            for (var frameIndex = 0; frameIndex < frames; frameIndex++)
            {
                if (!window.PumpMessages())
                {
                    break;
                }

                bridge.BeginFrame(device);
                bridge.SetRenderTargets(device, rtv);
                bridge.ClearRenderTarget(rtv, color);
                bridge.EndFrame(device);
                bridge.Present(swapChain, 1);

                if (frameIndex + 1 == frames)
                {
                    window.Destroy();
                }
            }

            Console.WriteLine("Done.");
        }
        finally
        {
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
}
