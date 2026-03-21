package examples;

import dxbridge.CommandLine;
import dxbridge.DXBridge;
import dxbridge.DXBridgeLibrary;
import dxbridge.Win32Window;

public final class Example05Dx11MovingTriangle {
    private static final String VS_SOURCE =
        "struct VSInput { float2 pos : POSITION; float3 col : COLOR; };\n"
        + "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
        + "PSInput main(VSInput v) {\n"
        + "    PSInput o;\n"
        + "    o.pos = float4(v.pos, 0.0f, 1.0f);\n"
        + "    o.col = v.col;\n"
        + "    return o;\n"
        + "}\n";

    private static final String PS_SOURCE =
        "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
        + "float4 main(PSInput p) : SV_TARGET {\n"
        + "    return float4(p.col, 1.0f);\n"
        + "}\n";

    public static void main(String[] args) {
        CommandLine cli = parse(args);
        if (cli.isHelpRequested()) {
            printUsage();
            return;
        }

        int width = cli.getInt("--width", 960);
        int height = cli.getInt("--height", 540);
        int frames = cli.getInt("--frames", 0);
        float speed = cli.getFloat("--speed", 1.6f);
        float amplitude = cli.getFloat("--amplitude", 0.38f);
        int syncInterval = cli.getInt("--sync-interval", 1);

        Win32Window window = new Win32Window(
            "DXBridge Java Moving Triangle",
            width,
            height,
            !cli.hasFlag("--hidden"),
            "DXBridgeJavaMovingTriangle"
        );
        DXBridge bridge = new DXBridge(cli.getString("--dll", null));

        long device = DXBridge.DXBRIDGE_NULL_HANDLE;
        long swapChain = DXBridge.DXBRIDGE_NULL_HANDLE;
        long rtv = DXBridge.DXBRIDGE_NULL_HANDLE;
        long vertexShader = DXBridge.DXBRIDGE_NULL_HANDLE;
        long pixelShader = DXBridge.DXBRIDGE_NULL_HANDLE;
        long inputLayout = DXBridge.DXBRIDGE_NULL_HANDLE;
        long pipeline = DXBridge.DXBRIDGE_NULL_HANDLE;
        long vertexBuffer = DXBridge.DXBRIDGE_NULL_HANDLE;
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

            vertexShader = bridge.compileShader(
                device,
                DXBridge.DXB_SHADER_STAGE_VERTEX,
                VS_SOURCE,
                "moving_triangle_vs",
                "main",
                "vs_5_0",
                DXBridge.DXB_COMPILE_DEBUG
            );
            pixelShader = bridge.compileShader(
                device,
                DXBridge.DXB_SHADER_STAGE_PIXEL,
                PS_SOURCE,
                "moving_triangle_ps",
                "main",
                "ps_5_0",
                DXBridge.DXB_COMPILE_DEBUG
            );

            DXBridgeLibrary.DXBInputElementDesc[] elements = DXBridge.newInputElementArray(2);
            elements[0].semantic = DXBridge.DXB_SEMANTIC_POSITION;
            elements[0].semantic_index = 0;
            elements[0].format = DXBridge.DXB_FORMAT_RG32_FLOAT;
            elements[0].input_slot = 0;
            elements[0].byte_offset = 0;

            elements[1].semantic = DXBridge.DXB_SEMANTIC_COLOR;
            elements[1].semantic_index = 0;
            elements[1].format = DXBridge.DXB_FORMAT_RGB32_FLOAT;
            elements[1].input_slot = 0;
            elements[1].byte_offset = 8;

            inputLayout = bridge.createInputLayout(device, elements, vertexShader);
            pipeline = bridge.createPipelineState(
                device,
                vertexShader,
                pixelShader,
                inputLayout,
                DXBridge.DXB_PRIM_TRIANGLELIST,
                0,
                0,
                0,
                0
            );
            vertexBuffer = bridge.createBuffer(
                device,
                DXBridge.DXB_BUFFER_FLAG_VERTEX | DXBridge.DXB_BUFFER_FLAG_DYNAMIC,
                15 * 4,
                5 * 4
            );

            long startTime = System.nanoTime();
            int frameIndex = 0;
            System.out.println("Rendering triangle animation. Close the window to exit.");

            while (window.pumpMessages()) {
                double elapsed = (System.nanoTime() - startTime) / 1000000000.0;
                float offsetX = (float) (amplitude * Math.sin(elapsed * speed));
                float offsetY = (float) (0.06 * Math.sin(elapsed * speed * 1.9));

                bridge.uploadFloats(vertexBuffer, makeVertexData(offsetX, offsetY));

                bridge.beginFrame(device);
                bridge.setRenderTargets(device, rtv, DXBridge.DXBRIDGE_NULL_HANDLE);
                bridge.clearRenderTarget(rtv, new float[] { 0.04f, 0.05f, 0.08f, 1.0f });
                bridge.setViewport(device, 0.0f, 0.0f, (float) width, (float) height, 0.0f, 1.0f);
                bridge.setPipeline(device, pipeline);
                bridge.setVertexBuffer(device, vertexBuffer, 5 * 4, 0);
                bridge.draw(device, 3, 0);
                bridge.endFrame(device);
                bridge.present(swapChain, Math.max(0, syncInterval));

                frameIndex++;
                if (frames > 0 && frameIndex >= frames) {
                    window.destroy();
                    break;
                }
            }

            System.out.println("Done. Rendered frames: " + frameIndex);
        } finally {
            if (vertexBuffer != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyBuffer(vertexBuffer);
            }
            if (pipeline != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyPipeline(pipeline);
            }
            if (inputLayout != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyInputLayout(inputLayout);
            }
            if (pixelShader != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyShader(pixelShader);
            }
            if (vertexShader != DXBridge.DXBRIDGE_NULL_HANDLE) {
                bridge.destroyShader(vertexShader);
            }
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

    private static float[] makeVertexData(float offsetX, float offsetY) {
        return new float[] {
            0.0f + offsetX, 0.52f + offsetY, 1.0f, 0.1f, 0.1f,
            -0.42f + offsetX, -0.38f + offsetY, 0.1f, 1.0f, 0.1f,
            0.42f + offsetX, -0.38f + offsetY, 0.1f, 0.3f, 1.0f
        };
    }

    private static CommandLine parse(String[] args) {
        return new CommandLine(
            args,
            CommandLine.options("--dll", "--width", "--height", "--frames", "--speed", "--amplitude", "--sync-interval"),
            CommandLine.options("--hidden", "--debug")
        );
    }

    private static void printUsage() {
        System.out.println(
            "Usage: Example05Dx11MovingTriangle [--dll PATH] [--width 960] [--height 540] [--frames 0] [--speed 1.6] [--amplitude 0.38] [--hidden] [--debug] [--sync-interval 1]"
        );
    }
}
