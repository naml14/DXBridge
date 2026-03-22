//go:build windows

package main

import (
	"flag"
	"fmt"
	"os"
	"strings"

	"dxbridgeexamples/internal/dxbridge"
)

type backendTarget struct {
	name  string
	id    uint32
	label string
}

var backends = []backendTarget{
	{name: "dx11", id: dxbridge.BackendDX11, label: "DX11"},
	{name: "dx12", id: dxbridge.BackendDX12, label: "DX12"},
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	dllPath := flag.String("dll", "", "Optional explicit path to dxbridge.dll")
	backendName := flag.String("backend", "all", "Backend set to inspect: all|dx11|dx12")
	compareActive := flag.String("compare-active", "", "Optionally initialize dx11 or dx12 after preflight to contrast DXBridge_SupportsFeature")
	flag.Parse()

	selectedBackends, err := selectBackends(*backendName)
	if err != nil {
		return err
	}
	compareBackend, err := selectCompareBackend(*compareActive)
	if err != nil {
		return err
	}

	lib, err := dxbridge.Load(*dllPath)
	if err != nil {
		return err
	}
	defer lib.Close()

	fmt.Printf("Loaded: %s\n", lib.Path)
	fmt.Println("Scenario 06 - Capability preflight")
	fmt.Println("Purpose: inspect backend readiness before DXBridge_Init().")
	fmt.Printf("Backends: %s\n", joinBackendLabels(selectedBackends))
	fmt.Println("Pre-init capability discovery:")
	for _, backend := range selectedBackends {
		if err := printBackend(lib, backend.id, backend.label); err != nil {
			return err
		}
	}
	if compareBackend != nil {
		if err := printLegacyComparison(lib, compareBackend.id, compareBackend.label); err != nil {
			return err
		}
	} else {
		fmt.Println("Tip: pass --compare-active dx11 or --compare-active dx12 to contrast the legacy active-backend check.")
	}
	return nil
}

func selectBackends(name string) ([]backendTarget, error) {
	if strings.EqualFold(name, "all") || strings.TrimSpace(name) == "" {
		return backends, nil
	}

	backend, err := dxbridge.ParseBackend(name)
	if err != nil {
		return nil, err
	}
	for _, candidate := range backends {
		if candidate.id == backend {
			return []backendTarget{candidate}, nil
		}
	}
	return nil, fmt.Errorf("unsupported backend for capability preflight: %s", name)
}

func selectCompareBackend(name string) (*backendTarget, error) {
	if strings.TrimSpace(name) == "" {
		return nil, nil
	}
	backend, err := dxbridge.ParseBackend(name)
	if err != nil {
		return nil, err
	}
	if backend == dxbridge.BackendAuto {
		return nil, fmt.Errorf("--compare-active must be dx11 or dx12")
	}
	for _, candidate := range backends {
		if candidate.id == backend {
			value := candidate
			return &value, nil
		}
	}
	return nil, fmt.Errorf("unsupported compare backend: %s", name)
}

func joinBackendLabels(selected []backendTarget) string {
	labels := make([]string, 0, len(selected))
	for _, backend := range selected {
		labels = append(labels, backend.label)
	}
	return strings.Join(labels, ", ")
}

func printLegacyComparison(lib *dxbridge.Library, backend uint32, label string) error {
	fmt.Println("Legacy comparison after DXBridge_Init():")
	if err := lib.Init(backend); err != nil {
		return err
	}
	defer lib.Shutdown()

	fmt.Printf("  active backend: %s\n", label)
	fmt.Printf("  DXBridge_SupportsFeature(DXB_FEATURE_DX12): %t\n", lib.SupportsFeature(dxbridge.FeatureDX12))
	fmt.Println("  This remains an active-backend check, not a machine-wide preflight query.")
	return nil
}

func printBackend(lib *dxbridge.Library, backend uint32, label string) error {
	available, err := lib.QueryBackendAvailable(backend)
	if err != nil {
		return err
	}
	debug, err := lib.QueryDebugLayerAvailable(backend)
	if err != nil {
		return err
	}
	gpu, err := lib.QueryGPUValidationAvailable(backend)
	if err != nil {
		return err
	}
	count, err := lib.QueryAdapterCount(backend)
	if err != nil {
		return err
	}

	fmt.Printf("%s:\n", label)
	fmt.Printf("  available: %t (best FL %s)\n", available.Supported != 0, dxbridge.FormatFeatureLevel(available.ValueU32))
	fmt.Printf("  debug layer: %t (best FL %s)\n", debug.Supported != 0, dxbridge.FormatFeatureLevel(debug.ValueU32))
	fmt.Printf("  GPU validation: %t (best FL %s)\n", gpu.Supported != 0, dxbridge.FormatFeatureLevel(gpu.ValueU32))
	fmt.Printf("  adapter count: %d\n", count.ValueU32)

	for adapterIndex := uint32(0); adapterIndex < count.ValueU32; adapterIndex++ {
		software, err := lib.QueryAdapterSoftware(backend, adapterIndex)
		if err != nil {
			return err
		}
		featureLevel, err := lib.QueryAdapterMaxFeatureLevel(backend, adapterIndex)
		if err != nil {
			return err
		}
		fmt.Printf("  adapter %d: software=%t maxFL=%s\n", adapterIndex, software.ValueU32 != 0, dxbridge.FormatFeatureLevel(featureLevel.ValueU32))
	}

	return nil
}
