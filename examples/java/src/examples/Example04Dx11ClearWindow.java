package examples;

import dxbridge.CommandLine;
import dxbridge.DXBridge;
import dxbridge.Win32Window;

public final class Example04Dx11ClearWindow {
    public static void main(String[] args) {
        CommandLine cli = parse(args);
        if (cli.isHelpRequested()) {
            printUsage();
            return;
        }

        int width = cli.getInt("--width", 640);
        int height = cli.getInt("--height", 360);
        int frames = cli.getInt("--frames", 180);
        float[] color = cli.getFloatList("--color", 4, new float[] { 0.08f, 0.16f, 0.32f, 1.0f });

        Win32Window window = new Win32Window(
            "DXBridge Java Clear Window",
            width,
            height,
            !cli.hasFlag("--hidden"),
            "DXBridgeJavaClearWindow"
        );
        DXBridge bridge = new DXBridge(cli.getString("--dll", null));

        long device = DXBridge.DXBRIDGE_NULL_HANDLE;
        long swapChain = DXBridge.DXBRIDGE_NULL_HANDLE;
        long rtv = DXBridge.DXBRIDGE_NULL_HANDLE;
        boolean initialized = false;

        try {
            window.create();
            System.out.println("Loaded: " + bridge.getDllPath().getAbsolutePath());
            System.out.printf("Created HWND: 0x%016X%n", window.getHwndValue());

            bridge.init(DXBridge.DXB_BACKEND_DX11);
            initialized = true;

            device = bridge.createDevice(
                DXBridge.DXB_BACKEND_DX11,
                0,
                cli.hasFlag("--debug") ? DXBridge.DXB_DEVICE_FLAG_DEBUG : 0
            );
            swapChain = bridge.createSwapChain(device, window.getHwndPointer(), width, height, DXBridge.DXB_FORMAT_RGBA8_UNORM, 2, 0);
            long backBuffer = bridge.getBackBuffer(swapChain);
            rtv = bridge.createRenderTargetView(device, backBuffer);

            System.out.println("Rendering frames. Close the window manually or wait for auto-close.");
            for (int frameIndex = 0; frameIndex < frames; frameIndex++) {
                if (!window.pumpMessages()) {
                    break;
                }
                bridge.beginFrame(device);
                bridge.setRenderTargets(device, rtv, DXBridge.DXBRIDGE_NULL_HANDLE);
                bridge.clearRenderTarget(rtv, color);
                bridge.endFrame(device);
                bridge.present(swapChain, 1);

                if (frameIndex + 1 == frames) {
                    window.destroy();
                }
            }

            System.out.println("Done.");
        } finally {
            if (rtv != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyRenderTargetView(rtv);
            }
            if (swapChain != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroySwapChain(swapChain);
            }
            if (device != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyDevice(device);
            }
            if (initialized) {
                bridge.shutdown();
            }
            window.destroy();
        }
    }

    private static CommandLine parse(String[] args) {
        return new CommandLine(
            args,
            CommandLine.options("--dll", "--width", "--height", "--frames", "--color"),
            CommandLine.options("--hidden", "--debug")
        );
    }

    private static void printUsage() {
        System.out.println(
            "Usage: Example04Dx11ClearWindow [--dll PATH] [--width 640] [--height 360] [--frames 180] [--color R,G,B,A] [--hidden] [--debug]"
        );
    }
}
