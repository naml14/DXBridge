using DXBridgeExamples.DxBridge;

namespace DXBridgeExamples.Examples;

internal static class Example02EnumerateAdapters
{
    public static int Run(string[] args)
    {
        var cli = new CommandLine(args, new[] { "--dll", "--backend" }, Array.Empty<string>());
        if (cli.HelpRequested)
        {
            Console.WriteLine("Usage: Example02EnumerateAdapters [--dll PATH] [--backend auto|dx11|dx12]");
            return 0;
        }

        using var bridge = new DXBridge(cli.GetString("--dll"));
        var initialized = false;
        try
        {
            Console.WriteLine($"Loaded: {bridge.DllPath}");
            bridge.Init(DXBridge.BackendFromName(cli.GetString("--backend", "auto")));
            initialized = true;

            var adapters = bridge.EnumerateAdapters();
            if (adapters.Count == 0)
            {
                Console.WriteLine("No adapters reported by DXBridge.");
                return 0;
            }

            Console.WriteLine($"Found {adapters.Count} adapter(s):");
            foreach (var adapter in adapters)
            {
                var kind = adapter.IsSoftware != 0 ? "software" : "hardware";
                Console.WriteLine($"- #{adapter.Index}: {DXBridge.AdapterDescription(adapter)} [{kind}] | VRAM={DXBridge.FormatBytes(adapter.DedicatedVideoMem)} | Shared={DXBridge.FormatBytes(adapter.SharedSystemMem)}");
            }
        }
        finally
        {
            if (initialized)
            {
                bridge.Shutdown();
            }
        }

        return 0;
    }
}
