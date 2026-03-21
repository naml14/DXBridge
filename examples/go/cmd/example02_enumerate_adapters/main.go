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
	backendName := flag.String("backend", "auto", "Backend hint used during DXBridge_Init")
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
	if err := lib.Init(backend); err != nil {
		return err
	}
	defer lib.Shutdown()

	adapters, err := lib.EnumerateAdapters()
	if err != nil {
		return err
	}
	if len(adapters) == 0 {
		fmt.Println("No adapters reported by DXBridge.")
		return nil
	}

	fmt.Printf("Found %d adapter(s):\n", len(adapters))
	for _, adapter := range adapters {
		kind := "hardware"
		if adapter.IsSoftware != 0 {
			kind = "software"
		}
		fmt.Printf("- #%d: %s [%s] | VRAM=%s | Shared=%s\n",
			adapter.Index,
			dxbridge.DecodeDescription(adapter.Description),
			kind,
			dxbridge.FormatBytes(adapter.DedicatedVideoMem),
			dxbridge.FormatBytes(adapter.SharedSystemMem),
		)
	}
	return nil
}
