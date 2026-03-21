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
	backendName := flag.String("backend", "auto", "Backend hint: auto|dx11|dx12")
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
	lib.SetLogCallback(func(level int, message string, _ uintptr) {
		label := "INFO"
		switch level {
		case 1:
			label = "WARN"
		case 2:
			label = "ERROR"
		}
		fmt.Printf("[log:%s] %s\n", label, message)
	})
	defer lib.SetLogCallback(nil)

	version := lib.GetVersion()
	fmt.Printf("DXBridge version: %s (0x%08X)\n", dxbridge.ParseVersion(version), version)
	if err := lib.Init(backend); err != nil {
		return err
	}
	defer lib.Shutdown()

	fmt.Printf("DX12 feature flag reported by active backend: %t\n", lib.SupportsFeature(dxbridge.FeatureDX12))
	fmt.Println("Initialization and shutdown completed successfully.")
	return nil
}
