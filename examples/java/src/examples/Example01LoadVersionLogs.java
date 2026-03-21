package examples;

import dxbridge.CommandLine;
import dxbridge.DXBridge;

public final class Example01LoadVersionLogs {
    public static void main(String[] args) {
        CommandLine cli = parse(args);
        if (cli.isHelpRequested()) {
            printUsage();
            return;
        }

        String dllPath = cli.getString("--dll", null);
        String backendName = cli.getString("--backend", "auto");
        DXBridge bridge = new DXBridge(dllPath);

        System.out.println("Loaded: " + bridge.getDllPath().getAbsolutePath());
        bridge.setLogCallback((level, message, userData) -> {
            System.out.println("[log:" + DXBridge.logLevelName(level) + "] " + message);
        });

        boolean initialized = false;
        try {
            int version = bridge.getVersion();
            System.out.println("DXBridge version: " + DXBridge.parseVersion(version) + " (" + DXBridge.formatHresult(version) + ")");

            bridge.init(DXBridge.backendFromName(backendName));
            initialized = true;

            System.out.println(
                "DX12 feature flag reported by active backend: " + bridge.supportsFeature(DXBridge.DXB_FEATURE_DX12)
            );
            System.out.println("Initialization and shutdown completed successfully.");
        } finally {
            if (initialized) {
                bridge.shutdown();
            }
            bridge.setLogCallback(null);
        }
    }

    private static CommandLine parse(String[] args) {
        return new CommandLine(args, CommandLine.options("--dll", "--backend"), CommandLine.options());
    }

    private static void printUsage() {
        System.out.println("Usage: Example01LoadVersionLogs [--dll PATH] [--backend auto|dx11|dx12]");
    }
}
