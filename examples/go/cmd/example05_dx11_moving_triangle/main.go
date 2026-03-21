//go:build windows

package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"time"

	"dxbridgeexamples/internal/dxbridge"
)

const vertexShaderSource = `
struct VSInput {
    float2 pos : POSITION;
    float3 col : COLOR;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

PSInput main(VSInput v) {
    PSInput o;
    o.pos = float4(v.pos, 0.0f, 1.0f);
    o.col = v.col;
    return o;
}
`

const pixelShaderSource = `
struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

float4 main(PSInput p) : SV_TARGET {
    return float4(p.col, 1.0f);
}
`

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	dllPath := flag.String("dll", "", "Optional explicit path to dxbridge.dll")
	width := flag.Int("width", 960, "Window width in pixels")
	height := flag.Int("height", 540, "Window height in pixels")
	frames := flag.Int("frames", 0, "How many frames to render (0 = until the window closes)")
	speed := flag.Float64("speed", 1.6, "Horizontal animation speed")
	amplitude := flag.Float64("amplitude", 0.38, "Horizontal movement amplitude in NDC")
	hidden := flag.Bool("hidden", false, "Create a real HWND without showing the window")
	debug := flag.Bool("debug", false, "Request DXB_DEVICE_FLAG_DEBUG")
	syncInterval := flag.Int("sync-interval", 1, "Present sync interval (1 = vsync, 0 = no vsync)")
	flag.Parse()

	window, err := dxbridge.NewWin32Window("DXBridge Go Moving Triangle", *width, *height, !*hidden, "DXBridgeGoMovingTriangle")
	if err != nil {
		return err
	}
	defer window.Destroy()

	lib, err := dxbridge.Load(*dllPath)
	if err != nil {
		return err
	}
	defer lib.Close()

	hwnd, err := window.Create()
	if err != nil {
		return err
	}
	fmt.Printf("Loaded: %s\n", lib.Path)
	fmt.Printf("Created HWND: 0x%016X\n", hwnd)

	if err := lib.Init(dxbridge.BackendDX11); err != nil {
		return err
	}
	defer lib.Shutdown()

	flags := uint32(0)
	if *debug {
		flags = dxbridge.DeviceFlagDebug
	}
	device, err := lib.CreateDevice(dxbridge.BackendDX11, 0, flags)
	if err != nil {
		return err
	}
	defer lib.DestroyDevice(device)

	swapChain, err := lib.CreateSwapChain(device, hwnd, uint32(*width), uint32(*height), dxbridge.FormatRGBA8Unorm, 2, 0)
	if err != nil {
		return err
	}
	defer lib.DestroySwapChain(swapChain)

	backBuffer, err := lib.GetBackBuffer(swapChain)
	if err != nil {
		return err
	}
	rtv, err := lib.CreateRenderTargetView(device, backBuffer)
	if err != nil {
		return err
	}
	defer lib.DestroyRenderTargetView(rtv)

	compileFlags := uint32(0)
	if *debug {
		compileFlags = dxbridge.CompileDebug
	}
	vs, err := lib.CompileShader(device, dxbridge.ShaderStageVertex, vertexShaderSource, "moving_triangle_vs", "main", "vs_5_0", compileFlags)
	if err != nil {
		return err
	}
	defer lib.DestroyShader(vs)

	ps, err := lib.CompileShader(device, dxbridge.ShaderStagePixel, pixelShaderSource, "moving_triangle_ps", "main", "ps_5_0", compileFlags)
	if err != nil {
		return err
	}
	defer lib.DestroyShader(ps)

	elements := []dxbridge.InputElementDesc{
		dxbridge.VersionedInputElement(dxbridge.SemanticPosition, 0, dxbridge.FormatRG32Float, 0, 0),
		dxbridge.VersionedInputElement(dxbridge.SemanticColor, 0, dxbridge.FormatRGB32Float, 0, 8),
	}
	inputLayout, err := lib.CreateInputLayout(device, elements, vs)
	if err != nil {
		return err
	}
	defer lib.DestroyInputLayout(inputLayout)

	pipeline, err := lib.CreatePipelineState(device, vs, ps, inputLayout, dxbridge.PrimTriangleList, 0, 0, 0, 0)
	if err != nil {
		return err
	}
	defer lib.DestroyPipeline(pipeline)

	vertexBuffer, err := lib.CreateBuffer(device, dxbridge.BufferFlagVertex|dxbridge.BufferFlagDynamic, 15*4, 5*4)
	if err != nil {
		return err
	}
	defer lib.DestroyBuffer(vertexBuffer)

	viewport := dxbridge.Viewport{
		X:        0,
		Y:        0,
		Width:    float32(*width),
		Height:   float32(*height),
		MinDepth: 0,
		MaxDepth: 1,
	}

	start := time.Now()
	rendered := 0
	fmt.Println("Rendering triangle animation. Close the window to exit.")
	for window.PumpMessages() {
		elapsed := time.Since(start).Seconds()
		offsetX := float32(*amplitude * math.Sin(elapsed**speed))
		offsetY := float32(0.06 * math.Sin(elapsed*(*speed*1.9)))
		vertices := makeVertexData(offsetX, offsetY)

		if err := lib.UploadFloats(vertexBuffer, vertices); err != nil {
			return err
		}
		if err := lib.BeginFrame(device); err != nil {
			return err
		}
		if err := lib.SetRenderTargets(device, rtv, dxbridge.NullHandle); err != nil {
			return err
		}
		if err := lib.ClearRenderTarget(rtv, [4]float32{0.04, 0.05, 0.08, 1.0}); err != nil {
			return err
		}
		if err := lib.SetViewport(device, viewport); err != nil {
			return err
		}
		if err := lib.SetPipeline(device, pipeline); err != nil {
			return err
		}
		if err := lib.SetVertexBuffer(device, vertexBuffer, 5*4, 0); err != nil {
			return err
		}
		if err := lib.Draw(device, 3, 0); err != nil {
			return err
		}
		if err := lib.EndFrame(device); err != nil {
			return err
		}
		if err := lib.Present(swapChain, uint32(maxInt(0, *syncInterval))); err != nil {
			return err
		}

		rendered++
		if *frames > 0 && rendered >= *frames {
			window.Close()
			break
		}
	}

	fmt.Printf("Done. Rendered frames: %d\n", rendered)
	return nil
}

func makeVertexData(offsetX float32, offsetY float32) []float32 {
	return []float32{
		0.0 + offsetX, 0.52 + offsetY, 1.0, 0.1, 0.1,
		-0.42 + offsetX, -0.38 + offsetY, 0.1, 1.0, 0.1,
		0.42 + offsetX, -0.38 + offsetY, 0.1, 0.3, 1.0,
	}
}

func maxInt(a int, b int) int {
	if a > b {
		return a
	}
	return b
}
