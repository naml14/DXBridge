//go:build windows

package main

import (
	"flag"
	"fmt"
	"os"

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
	backendName := flag.String("backend", "dx11", "Backend used for the successful CreateDevice call")
	debug := flag.Bool("debug", false, "Request DXB_DEVICE_FLAG_DEBUG on the successful CreateDevice call")
	flag.Parse()

	backend, err := dxbridge.ParseBackend(*backendName)
	if err != nil {
		return err
	}

	lib, err := dxbridge.Load(*dllPath)
	if err != nil {
		return err
	}
	defer lib.Close()

	fmt.Printf("Loaded: %s\n", lib.Path)
	fmt.Println("1) Intentionally failing CreateDevice before DXBridge_Init...")
	var notInitializedDevice uint64
	result := lib.CreateDeviceCall(nil, &notInitializedDevice)
	fmt.Printf("   result=%d | %s\n", int32(result), lib.DescribeFailure(result))
	fmt.Println("   Read GetLastError immediately: the buffer is thread-local and later calls can overwrite it.")

	fmt.Println("2) Initializing DXBridge...")
	if err := lib.Init(backend); err != nil {
		return err
	}
	defer lib.Shutdown()

	fmt.Println("3) Creating a real device...")
	flags := uint32(0)
	if *debug {
		flags = dxbridge.DeviceFlagDebug
	}
	device, err := lib.CreateDevice(backend, 0, flags)
	if err != nil {
		return err
	}
	defer lib.DestroyDevice(device)

	fmt.Printf("   device handle: %d\n", device)
	fmt.Printf("   feature level: %s\n", dxbridge.FormatHRESULT(lib.GetFeatureLevel(device)))
	fmt.Println("   Device creation succeeded.")
	return nil
}
