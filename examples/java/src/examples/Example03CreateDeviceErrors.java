package examples;

import com.sun.jna.ptr.LongByReference;

import dxbridge.CommandLine;
import dxbridge.DXBridge;
import dxbridge.DXBridgeLibrary;

public final class Example03CreateDeviceErrors {
    public static void main(String[] args) {
        CommandLine cli = parse(args);
        if (cli.isHelpRequested()) {
            printUsage();
            return;
        }

        DXBridge bridge = new DXBridge(cli.getString("--dll", null));
        System.out.println("Loaded: " + bridge.getDllPath().getAbsolutePath());

        System.out.println("1) Intentionally failing CreateDevice before DXBridge_Init...");
        LongByReference beforeInitHandle = new LongByReference(DXBridge.DXBRIDGE_NULL_HANDLE);
        int beforeInitResult = bridge.api().DXBridge_CreateDevice((DXBridgeLibrary.DXBDeviceDesc) null, beforeInitHandle);
        System.out.println("   result=" + beforeInitResult + " | " + bridge.describeFailure(beforeInitResult));
        System.out.println("   Read GetLastError immediately: the buffer is thread-local and later calls can overwrite it.");

        boolean initialized = false;
        long device = DXBridge.DXBRIDGE_NULL_HANDLE;

        try {
            System.out.println("2) Initializing DXBridge...");
            bridge.init(DXBridge.backendFromName(cli.getString("--backend", "dx11")));
            initialized = true;

            System.out.println("3) Creating a real device...");
            int flags = cli.hasFlag("--debug") ? DXBridge.DXB_DEVICE_FLAG_DEBUG : 0;
            device = bridge.createDevice(DXBridge.backendFromName(cli.getString("--backend", "dx11")), 0, flags);

            System.out.println("   device handle: " + Long.toUnsignedString(device));
            System.out.println("   feature level: " + DXBridge.formatHresult(bridge.getFeatureLevel(device)));
            System.out.println("   Device creation succeeded.");
        } finally {
            if (device != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyDevice(device);
            }
            if (initialized) {
                bridge.shutdown();
            }
        }
    }

    private static CommandLine parse(String[] args) {
        return new CommandLine(args, CommandLine.options("--dll", "--backend"), CommandLine.options("--debug"));
    }

    private static void printUsage() {
        System.out.println("Usage: Example03CreateDeviceErrors [--dll PATH] [--backend auto|dx11|dx12] [--debug]");
    }
}
