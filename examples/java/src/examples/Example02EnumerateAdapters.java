package examples;

import java.util.List;

import dxbridge.CommandLine;
import dxbridge.DXBridge;
import dxbridge.DXBridgeLibrary;

public final class Example02EnumerateAdapters {
    public static void main(String[] args) {
        CommandLine cli = parse(args);
        if (cli.isHelpRequested()) {
            printUsage();
            return;
        }

        DXBridge bridge = new DXBridge(cli.getString("--dll", null));
        boolean initialized = false;

        try {
            System.out.println("Loaded: " + bridge.getDllPath().getAbsolutePath());
            bridge.init(DXBridge.backendFromName(cli.getString("--backend", "auto")));
            initialized = true;

            List<DXBridgeLibrary.DXBAdapterInfo> adapters = bridge.enumerateAdapters();
            if (adapters.isEmpty()) {
                System.out.println("No adapters reported by DXBridge.");
                return;
            }

            System.out.println("Found " + adapters.size() + " adapter(s):");
            for (DXBridgeLibrary.DXBAdapterInfo adapter : adapters) {
                String kind = adapter.is_software != 0 ? "software" : "hardware";
                System.out.println(
                    "- #" + adapter.index + ": " + DXBridge.adapterDescription(adapter)
                    + " [" + kind + "] | VRAM=" + DXBridge.formatBytes(adapter.dedicated_video_mem)
                    + " | Shared=" + DXBridge.formatBytes(adapter.shared_system_mem)
                );
            }
        } finally {
            if (initialized) {
                bridge.shutdown();
            }
        }
    }

    private static CommandLine parse(String[] args) {
        return new CommandLine(args, CommandLine.options("--dll", "--backend"), CommandLine.options());
    }

    private static void printUsage() {
        System.out.println("Usage: Example02EnumerateAdapters [--dll PATH] [--backend auto|dx11|dx12]");
    }
}
