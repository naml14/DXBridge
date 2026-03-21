//go:build windows

package main

import (
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"

	"dxbridgeexamples/internal/dxbridge"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	dllPath := flag.String("dll", "", "Optional explicit path to dxbridge.dll")
	width := flag.Int("width", 640, "Window width in pixels")
	height := flag.Int("height", 360, "Window height in pixels")
	frames := flag.Int("frames", 180, "How many frames to render before auto-close")
	colorText := flag.String("color", "0.08,0.16,0.32,1.0", "Clear color as R,G,B,A")
	hidden := flag.Bool("hidden", false, "Create a real HWND without showing the window")
	debug := flag.Bool("debug", false, "Request DXB_DEVICE_FLAG_DEBUG")
	flag.Parse()

	clearColor, err := parseRGBA(*colorText)
	if err != nil {
		return err
	}

	window, err := dxbridge.NewWin32Window("DXBridge Go Clear Window", *width, *height, !*hidden, "DXBridgeGoClearWindow")
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

	fmt.Println("Rendering frames. Close the window manually or wait for auto-close.")
	for frame := 0; frame < *frames; frame++ {
		if !window.PumpMessages() {
			break
		}
		if err := lib.BeginFrame(device); err != nil {
			return err
		}
		if err := lib.SetRenderTargets(device, rtv, dxbridge.NullHandle); err != nil {
			return err
		}
		if err := lib.ClearRenderTarget(rtv, clearColor); err != nil {
			return err
		}
		if err := lib.EndFrame(device); err != nil {
			return err
		}
		if err := lib.Present(swapChain, 1); err != nil {
			return err
		}
		if frame+1 == *frames {
			window.Close()
		}
	}

	fmt.Println("Done.")
	return nil
}

func parseRGBA(value string) ([4]float32, error) {
	parts := strings.Split(value, ",")
	if len(parts) != 4 {
		return [4]float32{}, fmt.Errorf("--color must be R,G,B,A")
	}
	var rgba [4]float32
	for i, part := range parts {
		parsed, err := strconv.ParseFloat(strings.TrimSpace(part), 32)
		if err != nil {
			return [4]float32{}, fmt.Errorf("invalid color component %q: %w", part, err)
		}
		rgba[i] = float32(parsed)
	}
	return rgba, nil
}
