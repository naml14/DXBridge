using DXBridgeExamples.DxBridge;

namespace DXBridgeExamples.Examples;

internal static class Example03CreateDeviceErrors
{
    public static int Run(string[] args)
    {
        var cli = new CommandLine(args, new[] { "--dll", "--backend" }, new[] { "--debug" });
        if (cli.HelpRequested)
        {
            Console.WriteLine("Usage: Example03CreateDeviceErrors [--dll PATH] [--backend auto|dx11|dx12] [--debug]");
            return 0;
        }

        using var bridge = new DXBridge(cli.GetString("--dll"));
        Console.WriteLine($"Loaded: {bridge.DllPath}");

        Console.WriteLine("1) Intentionally failing CreateDevice before DXBridge_Init...");
        var beforeInitResult = bridge.CreateDeviceRaw(IntPtr.Zero, out _);
        Console.WriteLine($"   result={beforeInitResult} | {bridge.DescribeFailure(beforeInitResult)}");
        Console.WriteLine("   Read GetLastError immediately: the buffer is thread-local and later calls can overwrite it.");

        var initialized = false;
        var device = DXBridge.NullHandle;
        try
        {
            var backend = DXBridge.BackendFromName(cli.GetString("--backend", "dx11"));
            Console.WriteLine("2) Initializing DXBridge...");
            bridge.Init(backend);
            initialized = true;

            Console.WriteLine("3) Creating a real device...");
            var flags = cli.HasFlag("--debug") ? DXBridge.DxbDeviceFlagDebug : 0u;
            device = bridge.CreateDevice(backend, 0, flags);
            Console.WriteLine($"   device handle: {device}");
            Console.WriteLine($"   feature level: {DXBridge.FormatHResult(bridge.GetFeatureLevel(device))}");
            Console.WriteLine("   Device creation succeeded.");
        }
        finally
        {
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
