using DXBridgeExamples.DxBridge;

namespace DXBridgeExamples.Examples;

internal static class Example01LoadVersionLogs
{
    public static int Run(string[] args)
    {
        var cli = new CommandLine(args, new[] { "--dll", "--backend" }, Array.Empty<string>());
        if (cli.HelpRequested)
        {
            Console.WriteLine("Usage: Example01LoadVersionLogs [--dll PATH] [--backend auto|dx11|dx12]");
            return 0;
        }

        using var bridge = new DXBridge(cli.GetString("--dll"));
        Console.WriteLine($"Loaded: {bridge.DllPath}");

        bridge.SetLogCallback((level, message, _) =>
        {
            Console.WriteLine($"[log:{DXBridge.LogLevelName(level)}] {message}");
        });

        var initialized = false;
        try
        {
            var version = bridge.GetVersion();
            Console.WriteLine($"DXBridge version: {DXBridge.ParseVersion(version)} ({DXBridge.FormatHResult(version)})");

            bridge.Init(DXBridge.BackendFromName(cli.GetString("--backend", "auto")));
            initialized = true;

            Console.WriteLine($"DX12 feature flag reported by active backend: {bridge.SupportsFeature(DXBridge.DxbFeatureDx12)}");
            Console.WriteLine("Initialization and shutdown completed successfully.");
        }
        finally
        {
            if (initialized)
            {
                bridge.Shutdown();
            }

            bridge.SetLogCallback(null);
        }

        return 0;
    }
}
