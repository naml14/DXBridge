//go:build windows

package dxbridge

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"unsafe"
)

const (
	NullHandle    uint64 = 0
	StructVersion uint32 = 1

	OK               Result = 0
	ErrInvalidHandle Result = -1
	ErrDeviceLost    Result = -2
	ErrOutOfMemory   Result = -3
	ErrInvalidArg    Result = -4
	ErrNotSupported  Result = -5
	ErrShaderCompile Result = -6
	ErrNotInit       Result = -7
	ErrInvalidState  Result = -8
	ErrInternal      Result = -999

	BackendAuto uint32 = 0
	BackendDX11 uint32 = 1
	BackendDX12 uint32 = 2

	FormatUnknown    uint32 = 0
	FormatRGBA8Unorm uint32 = 1
	FormatD32Float   uint32 = 3
	FormatRG32Float  uint32 = 6
	FormatRGB32Float uint32 = 7

	DeviceFlagDebug         uint32 = 0x0001
	DeviceFlagGPUValidation uint32 = 0x0002

	BufferFlagVertex  uint32 = 0x0001
	BufferFlagDynamic uint32 = 0x0008

	ShaderStageVertex uint32 = 0
	ShaderStagePixel  uint32 = 1

	PrimTriangleList uint32 = 0

	SemanticPosition uint32 = 0
	SemanticColor    uint32 = 3

	CompileDebug uint32 = 0x0001
	FeatureDX12  uint32 = 0x0001
)

type Result int32

type AdapterInfo struct {
	StructVersion      uint32
	Index              uint32
	Description        [128]byte
	DedicatedVideoMem  uint64
	DedicatedSystemMem uint64
	SharedSystemMem    uint64
	IsSoftware         uint32
}

type DeviceDesc struct {
	StructVersion uint32
	Backend       uint32
	AdapterIndex  uint32
	Flags         uint32
}

type SwapChainDesc struct {
	StructVersion uint32
	HWND          uintptr
	Width         uint32
	Height        uint32
	Format        uint32
	BufferCount   uint32
	Flags         uint32
}

type BufferDesc struct {
	StructVersion uint32
	Flags         uint32
	ByteSize      uint32
	Stride        uint32
}

type ShaderDesc struct {
	StructVersion uint32
	Stage         uint32
	SourceCode    *byte
	SourceSize    uint32
	SourceName    *byte
	EntryPoint    *byte
	Target        *byte
	CompileFlags  uint32
}

type InputElementDesc struct {
	StructVersion uint32
	Semantic      uint32
	SemanticIndex uint32
	Format        uint32
	InputSlot     uint32
	ByteOffset    uint32
}

type PipelineDesc struct {
	StructVersion uint32
	VS            uint64
	PS            uint64
	InputLayout   uint64
	Topology      uint32
	DepthTest     uint32
	DepthWrite    uint32
	CullBack      uint32
	Wireframe     uint32
}

type Viewport struct {
	X        float32
	Y        float32
	Width    float32
	Height   float32
	MinDepth float32
	MaxDepth float32
}

type Library struct {
	Path                   string
	dll                    *syscall.DLL
	setLogCallbackProc     *syscall.Proc
	initProc               *syscall.Proc
	shutdownProc           *syscall.Proc
	getVersionProc         *syscall.Proc
	supportsFeatureProc    *syscall.Proc
	getLastErrorProc       *syscall.Proc
	getLastHRESULTProc     *syscall.Proc
	enumerateAdaptersProc  *syscall.Proc
	createDeviceProc       *syscall.Proc
	destroyDeviceProc      *syscall.Proc
	getFeatureLevelProc    *syscall.Proc
	createSwapChainProc    *syscall.Proc
	destroySwapChainProc   *syscall.Proc
	getBackBufferProc      *syscall.Proc
	createRTVProc          *syscall.Proc
	destroyRTVProc         *syscall.Proc
	compileShaderProc      *syscall.Proc
	destroyShaderProc      *syscall.Proc
	createInputLayoutProc  *syscall.Proc
	destroyInputLayoutProc *syscall.Proc
	createPipelineProc     *syscall.Proc
	destroyPipelineProc    *syscall.Proc
	createBufferProc       *syscall.Proc
	uploadDataProc         *syscall.Proc
	destroyBufferProc      *syscall.Proc
	beginFrameProc         *syscall.Proc
	setRenderTargetsProc   *syscall.Proc
	clearRenderTargetProc  *syscall.Proc
	setViewportProc        *syscall.Proc
	setPipelineProc        *syscall.Proc
	setVertexBufferProc    *syscall.Proc
	drawProc               *syscall.Proc
	endFrameProc           *syscall.Proc
	presentProc            *syscall.Proc
}

type LogHandler func(level int, message string, userData uintptr)

var (
	logCallbackMu      sync.Mutex
	activeLogCallback  LogHandler
	logCallbackPointer = syscall.NewCallbackCDecl(logCallbackThunk)
)

func Load(explicitPath string) (*Library, error) {
	path, err := locateDLL(explicitPath)
	if err != nil {
		return nil, err
	}

	dll, err := syscall.LoadDLL(path)
	if err != nil {
		return nil, fmt.Errorf("load %s: %w", path, err)
	}

	loadProc := func(name string) (*syscall.Proc, error) {
		proc, procErr := dll.FindProc(name)
		if procErr != nil {
			_ = dll.Release()
			return nil, fmt.Errorf("find %s: %w", name, procErr)
		}
		return proc, nil
	}

	lib := &Library{Path: path, dll: dll}
	if lib.setLogCallbackProc, err = loadProc("DXBridge_SetLogCallback"); err != nil {
		return nil, err
	}
	if lib.initProc, err = loadProc("DXBridge_Init"); err != nil {
		return nil, err
	}
	if lib.shutdownProc, err = loadProc("DXBridge_Shutdown"); err != nil {
		return nil, err
	}
	if lib.getVersionProc, err = loadProc("DXBridge_GetVersion"); err != nil {
		return nil, err
	}
	if lib.supportsFeatureProc, err = loadProc("DXBridge_SupportsFeature"); err != nil {
		return nil, err
	}
	if lib.getLastErrorProc, err = loadProc("DXBridge_GetLastError"); err != nil {
		return nil, err
	}
	if lib.getLastHRESULTProc, err = loadProc("DXBridge_GetLastHRESULT"); err != nil {
		return nil, err
	}
	if lib.enumerateAdaptersProc, err = loadProc("DXBridge_EnumerateAdapters"); err != nil {
		return nil, err
	}
	if lib.createDeviceProc, err = loadProc("DXBridge_CreateDevice"); err != nil {
		return nil, err
	}
	if lib.destroyDeviceProc, err = loadProc("DXBridge_DestroyDevice"); err != nil {
		return nil, err
	}
	if lib.getFeatureLevelProc, err = loadProc("DXBridge_GetFeatureLevel"); err != nil {
		return nil, err
	}
	if lib.createSwapChainProc, err = loadProc("DXBridge_CreateSwapChain"); err != nil {
		return nil, err
	}
	if lib.destroySwapChainProc, err = loadProc("DXBridge_DestroySwapChain"); err != nil {
		return nil, err
	}
	if lib.getBackBufferProc, err = loadProc("DXBridge_GetBackBuffer"); err != nil {
		return nil, err
	}
	if lib.createRTVProc, err = loadProc("DXBridge_CreateRenderTargetView"); err != nil {
		return nil, err
	}
	if lib.destroyRTVProc, err = loadProc("DXBridge_DestroyRTV"); err != nil {
		return nil, err
	}
	if lib.compileShaderProc, err = loadProc("DXBridge_CompileShader"); err != nil {
		return nil, err
	}
	if lib.destroyShaderProc, err = loadProc("DXBridge_DestroyShader"); err != nil {
		return nil, err
	}
	if lib.createInputLayoutProc, err = loadProc("DXBridge_CreateInputLayout"); err != nil {
		return nil, err
	}
	if lib.destroyInputLayoutProc, err = loadProc("DXBridge_DestroyInputLayout"); err != nil {
		return nil, err
	}
	if lib.createPipelineProc, err = loadProc("DXBridge_CreatePipelineState"); err != nil {
		return nil, err
	}
	if lib.destroyPipelineProc, err = loadProc("DXBridge_DestroyPipeline"); err != nil {
		return nil, err
	}
	if lib.createBufferProc, err = loadProc("DXBridge_CreateBuffer"); err != nil {
		return nil, err
	}
	if lib.uploadDataProc, err = loadProc("DXBridge_UploadData"); err != nil {
		return nil, err
	}
	if lib.destroyBufferProc, err = loadProc("DXBridge_DestroyBuffer"); err != nil {
		return nil, err
	}
	if lib.beginFrameProc, err = loadProc("DXBridge_BeginFrame"); err != nil {
		return nil, err
	}
	if lib.setRenderTargetsProc, err = loadProc("DXBridge_SetRenderTargets"); err != nil {
		return nil, err
	}
	if lib.clearRenderTargetProc, err = loadProc("DXBridge_ClearRenderTarget"); err != nil {
		return nil, err
	}
	if lib.setViewportProc, err = loadProc("DXBridge_SetViewport"); err != nil {
		return nil, err
	}
	if lib.setPipelineProc, err = loadProc("DXBridge_SetPipeline"); err != nil {
		return nil, err
	}
	if lib.setVertexBufferProc, err = loadProc("DXBridge_SetVertexBuffer"); err != nil {
		return nil, err
	}
	if lib.drawProc, err = loadProc("DXBridge_Draw"); err != nil {
		return nil, err
	}
	if lib.endFrameProc, err = loadProc("DXBridge_EndFrame"); err != nil {
		return nil, err
	}
	if lib.presentProc, err = loadProc("DXBridge_Present"); err != nil {
		return nil, err
	}

	return lib, nil

}

func (l *Library) Close() error {
	l.SetLogCallback(nil)
	if l.dll == nil {
		return nil
	}
	err := l.dll.Release()
	l.dll = nil
	return err
}

func (l *Library) SetLogCallback(handler LogHandler) {
	logCallbackMu.Lock()
	activeLogCallback = handler
	logCallbackMu.Unlock()

	callback := uintptr(0)
	if handler != nil {
		callback = logCallbackPointer
	}
	l.setLogCallbackProc.Call(callback, 0)
}

func (l *Library) GetVersion() uint32 {
	r1, _, _ := l.getVersionProc.Call()
	return uint32(r1)
}

func (l *Library) SupportsFeature(featureFlag uint32) bool {
	r1, _, _ := l.supportsFeatureProc.Call(uintptr(featureFlag))
	return r1 != 0
}

func (l *Library) Init(backend uint32) error {
	return l.requireOK(l.callResult(l.initProc, uintptr(backend)), "DXBridge_Init")
}

func (l *Library) Shutdown() {
	l.shutdownProc.Call()
}

func (l *Library) GetLastError() string {
	buffer := make([]byte, 512)
	l.getLastErrorProc.Call(uintptr(unsafe.Pointer(&buffer[0])), uintptr(len(buffer)))
	end := 0
	for end < len(buffer) && buffer[end] != 0 {
		end++
	}
	return string(buffer[:end])
}

func (l *Library) GetLastHRESULT() uint32 {
	r1, _, _ := l.getLastHRESULTProc.Call()
	return uint32(r1)
}

func (l *Library) DescribeFailure(result Result) string {
	message := l.GetLastError()
	hresult := FormatHRESULT(l.GetLastHRESULT())
	name := ResultName(result)
	if message == "" {
		return fmt.Sprintf("%s (HRESULT %s)", name, hresult)
	}
	return fmt.Sprintf("%s: %s (HRESULT %s)", name, message, hresult)
}

func (l *Library) EnumerateAdapters() ([]AdapterInfo, error) {
	count := uint32(0)
	if err := l.requireOK(l.callResult(l.enumerateAdaptersProc, 0, uintptr(unsafe.Pointer(&count))), "DXBridge_EnumerateAdapters(count)"); err != nil {
		return nil, err
	}
	if count == 0 {
		return nil, nil
	}

	items := make([]AdapterInfo, count)
	for i := range items {
		items[i].StructVersion = StructVersion
	}
	if err := l.requireOK(l.callResult(l.enumerateAdaptersProc, uintptr(unsafe.Pointer(&items[0])), uintptr(unsafe.Pointer(&count))), "DXBridge_EnumerateAdapters(fill)"); err != nil {
		return nil, err
	}
	return items[:count], nil
}

func (l *Library) CreateDeviceCall(desc *DeviceDesc, outDevice *uint64) Result {
	return l.callResult(l.createDeviceProc, ptr(desc), ptr(outDevice))
}

func (l *Library) CreateDevice(backend uint32, adapterIndex uint32, flags uint32) (uint64, error) {
	desc := DeviceDesc{
		StructVersion: StructVersion,
		Backend:       backend,
		AdapterIndex:  adapterIndex,
		Flags:         flags,
	}
	device := uint64(0)
	if err := l.requireOK(l.CreateDeviceCall(&desc, &device), "DXBridge_CreateDevice"); err != nil {
		return 0, err
	}
	return device, nil
}

func (l *Library) DestroyDevice(device uint64) {
	l.destroyDeviceProc.Call(uintptr(device))
}

func (l *Library) GetFeatureLevel(device uint64) uint32 {
	r1, _, _ := l.getFeatureLevelProc.Call(uintptr(device))
	return uint32(r1)
}

func (l *Library) CreateSwapChain(device uint64, hwnd uintptr, width uint32, height uint32, format uint32, bufferCount uint32, flags uint32) (uint64, error) {
	desc := SwapChainDesc{
		StructVersion: StructVersion,
		HWND:          hwnd,
		Width:         width,
		Height:        height,
		Format:        format,
		BufferCount:   bufferCount,
		Flags:         flags,
	}
	swapChain := uint64(0)
	if err := l.requireOK(l.callResult(l.createSwapChainProc, uintptr(device), uintptr(unsafe.Pointer(&desc)), uintptr(unsafe.Pointer(&swapChain))), "DXBridge_CreateSwapChain"); err != nil {
		return 0, err
	}
	return swapChain, nil
}

func (l *Library) DestroySwapChain(swapChain uint64) {
	l.destroySwapChainProc.Call(uintptr(swapChain))
}

func (l *Library) GetBackBuffer(swapChain uint64) (uint64, error) {
	renderTarget := uint64(0)
	if err := l.requireOK(l.callResult(l.getBackBufferProc, uintptr(swapChain), uintptr(unsafe.Pointer(&renderTarget))), "DXBridge_GetBackBuffer"); err != nil {
		return 0, err
	}
	return renderTarget, nil
}

func (l *Library) CreateRenderTargetView(device uint64, renderTarget uint64) (uint64, error) {
	rtv := uint64(0)
	if err := l.requireOK(l.callResult(l.createRTVProc, uintptr(device), uintptr(renderTarget), uintptr(unsafe.Pointer(&rtv))), "DXBridge_CreateRenderTargetView"); err != nil {
		return 0, err
	}
	return rtv, nil
}

func (l *Library) DestroyRenderTargetView(rtv uint64) {
	l.destroyRTVProc.Call(uintptr(rtv))
}

func (l *Library) CompileShader(device uint64, stage uint32, sourceCode string, sourceName string, entryPoint string, target string, compileFlags uint32) (uint64, error) {
	sourceBytes, err := syscall.BytePtrFromString(sourceCode)
	if err != nil {
		return 0, err
	}
	nameBytes, err := syscall.BytePtrFromString(sourceName)
	if err != nil {
		return 0, err
	}
	entryBytes, err := syscall.BytePtrFromString(entryPoint)
	if err != nil {
		return 0, err
	}
	targetBytes, err := syscall.BytePtrFromString(target)
	if err != nil {
		return 0, err
	}
	desc := ShaderDesc{
		StructVersion: StructVersion,
		Stage:         stage,
		SourceCode:    sourceBytes,
		SourceSize:    0,
		SourceName:    nameBytes,
		EntryPoint:    entryBytes,
		Target:        targetBytes,
		CompileFlags:  compileFlags,
	}
	shader := uint64(0)
	if err := l.requireOK(l.callResult(l.compileShaderProc, uintptr(device), uintptr(unsafe.Pointer(&desc)), uintptr(unsafe.Pointer(&shader))), "DXBridge_CompileShader"); err != nil {
		return 0, err
	}
	return shader, nil
}

func (l *Library) DestroyShader(shader uint64) {
	l.destroyShaderProc.Call(uintptr(shader))
}

func (l *Library) CreateInputLayout(device uint64, elements []InputElementDesc, vertexShader uint64) (uint64, error) {
	if len(elements) == 0 {
		return 0, fmt.Errorf("elements must not be empty")
	}
	inputLayout := uint64(0)
	if err := l.requireOK(l.callResult(l.createInputLayoutProc, uintptr(device), uintptr(unsafe.Pointer(&elements[0])), uintptr(len(elements)), uintptr(vertexShader), uintptr(unsafe.Pointer(&inputLayout))), "DXBridge_CreateInputLayout"); err != nil {
		return 0, err
	}
	return inputLayout, nil
}

func (l *Library) DestroyInputLayout(inputLayout uint64) {
	l.destroyInputLayoutProc.Call(uintptr(inputLayout))
}

func (l *Library) CreatePipelineState(device uint64, vertexShader uint64, pixelShader uint64, inputLayout uint64, topology uint32, depthTest uint32, depthWrite uint32, cullBack uint32, wireframe uint32) (uint64, error) {
	desc := PipelineDesc{
		StructVersion: StructVersion,
		VS:            vertexShader,
		PS:            pixelShader,
		InputLayout:   inputLayout,
		Topology:      topology,
		DepthTest:     depthTest,
		DepthWrite:    depthWrite,
		CullBack:      cullBack,
		Wireframe:     wireframe,
	}
	pipeline := uint64(0)
	if err := l.requireOK(l.callResult(l.createPipelineProc, uintptr(device), uintptr(unsafe.Pointer(&desc)), uintptr(unsafe.Pointer(&pipeline))), "DXBridge_CreatePipelineState"); err != nil {
		return 0, err
	}
	return pipeline, nil
}

func (l *Library) DestroyPipeline(pipeline uint64) {
	l.destroyPipelineProc.Call(uintptr(pipeline))
}

func (l *Library) CreateBuffer(device uint64, flags uint32, byteSize uint32, stride uint32) (uint64, error) {
	desc := BufferDesc{
		StructVersion: StructVersion,
		Flags:         flags,
		ByteSize:      byteSize,
		Stride:        stride,
	}
	buffer := uint64(0)
	if err := l.requireOK(l.callResult(l.createBufferProc, uintptr(device), uintptr(unsafe.Pointer(&desc)), uintptr(unsafe.Pointer(&buffer))), "DXBridge_CreateBuffer"); err != nil {
		return 0, err
	}
	return buffer, nil
}

func (l *Library) UploadFloats(buffer uint64, values []float32) error {
	if len(values) == 0 {
		return fmt.Errorf("values must not be empty")
	}
	byteSize := uint32(len(values) * 4)
	return l.requireOK(l.callResult(l.uploadDataProc, uintptr(buffer), uintptr(unsafe.Pointer(&values[0])), uintptr(byteSize)), "DXBridge_UploadData")
}

func (l *Library) DestroyBuffer(buffer uint64) {
	l.destroyBufferProc.Call(uintptr(buffer))
}

func (l *Library) BeginFrame(device uint64) error {
	return l.requireOK(l.callResult(l.beginFrameProc, uintptr(device)), "DXBridge_BeginFrame")
}

func (l *Library) SetRenderTargets(device uint64, rtv uint64, dsv uint64) error {
	return l.requireOK(l.callResult(l.setRenderTargetsProc, uintptr(device), uintptr(rtv), uintptr(dsv)), "DXBridge_SetRenderTargets")
}

func (l *Library) ClearRenderTarget(rtv uint64, rgba [4]float32) error {
	return l.requireOK(l.callResult(l.clearRenderTargetProc, uintptr(rtv), uintptr(unsafe.Pointer(&rgba[0]))), "DXBridge_ClearRenderTarget")
}

func (l *Library) SetViewport(device uint64, viewport Viewport) error {
	return l.requireOK(l.callResult(l.setViewportProc, uintptr(device), uintptr(unsafe.Pointer(&viewport))), "DXBridge_SetViewport")
}

func (l *Library) SetPipeline(device uint64, pipeline uint64) error {
	return l.requireOK(l.callResult(l.setPipelineProc, uintptr(device), uintptr(pipeline)), "DXBridge_SetPipeline")
}

func (l *Library) SetVertexBuffer(device uint64, buffer uint64, stride uint32, offset uint32) error {
	return l.requireOK(l.callResult(l.setVertexBufferProc, uintptr(device), uintptr(buffer), uintptr(stride), uintptr(offset)), "DXBridge_SetVertexBuffer")
}

func (l *Library) Draw(device uint64, vertexCount uint32, startVertex uint32) error {
	return l.requireOK(l.callResult(l.drawProc, uintptr(device), uintptr(vertexCount), uintptr(startVertex)), "DXBridge_Draw")
}

func (l *Library) EndFrame(device uint64) error {
	return l.requireOK(l.callResult(l.endFrameProc, uintptr(device)), "DXBridge_EndFrame")
}

func (l *Library) Present(swapChain uint64, syncInterval uint32) error {
	return l.requireOK(l.callResult(l.presentProc, uintptr(swapChain), uintptr(syncInterval)), "DXBridge_Present")
}

func (l *Library) callResult(proc *syscall.Proc, args ...uintptr) Result {
	r1, _, _ := proc.Call(args...)
	return Result(int32(r1))
}

func (l *Library) requireOK(result Result, functionName string) error {
	if result == OK {
		return nil
	}
	return fmt.Errorf("%s failed: %s", functionName, l.DescribeFailure(result))
}

func ResultName(result Result) string {
	switch result {
	case OK:
		return "DXB_OK"
	case ErrInvalidHandle:
		return "DXB_E_INVALID_HANDLE"
	case ErrDeviceLost:
		return "DXB_E_DEVICE_LOST"
	case ErrOutOfMemory:
		return "DXB_E_OUT_OF_MEMORY"
	case ErrInvalidArg:
		return "DXB_E_INVALID_ARG"
	case ErrNotSupported:
		return "DXB_E_NOT_SUPPORTED"
	case ErrShaderCompile:
		return "DXB_E_SHADER_COMPILE"
	case ErrNotInit:
		return "DXB_E_NOT_INITIALIZED"
	case ErrInvalidState:
		return "DXB_E_INVALID_STATE"
	case ErrInternal:
		return "DXB_E_INTERNAL"
	default:
		return fmt.Sprintf("DXBResult(%d)", int32(result))
	}
}

func ParseVersion(version uint32) string {
	major := (version >> 16) & 0xFF
	minor := (version >> 8) & 0xFF
	patch := version & 0xFF
	return fmt.Sprintf("%d.%d.%d", major, minor, patch)
}

func ParseBackend(name string) (uint32, error) {
	switch strings.ToLower(name) {
	case "auto":
		return BackendAuto, nil
	case "dx11":
		return BackendDX11, nil
	case "dx12":
		return BackendDX12, nil
	default:
		return 0, fmt.Errorf("unknown backend %q", name)
	}
}

func FormatBytes(value uint64) string {
	units := []string{"B", "KiB", "MiB", "GiB", "TiB"}
	amount := float64(value)
	unit := units[0]
	for _, candidate := range units {
		unit = candidate
		if amount < 1024 || candidate == units[len(units)-1] {
			break
		}
		amount /= 1024
	}
	if unit == "B" {
		return fmt.Sprintf("%d %s", value, unit)
	}
	return fmt.Sprintf("%.1f %s", amount, unit)
}

func FormatHRESULT(value uint32) string {
	return fmt.Sprintf("0x%08X", value)
}

func DecodeDescription(raw [128]byte) string {
	end := 0
	for end < len(raw) && raw[end] != 0 {
		end++
	}
	return string(raw[:end])
}

func VersionedInputElement(semantic uint32, semanticIndex uint32, format uint32, inputSlot uint32, byteOffset uint32) InputElementDesc {
	return InputElementDesc{
		StructVersion: StructVersion,
		Semantic:      semantic,
		SemanticIndex: semanticIndex,
		Format:        format,
		InputSlot:     inputSlot,
		ByteOffset:    byteOffset,
	}
}

func logCallbackThunk(level uintptr, message uintptr, userData uintptr) uintptr {
	logCallbackMu.Lock()
	handler := activeLogCallback
	logCallbackMu.Unlock()
	if handler != nil {
		handler(int(level), cString(message), userData)
	}
	return 0
}

func cString(ptrValue uintptr) string {
	if ptrValue == 0 {
		return ""
	}
	data := make([]byte, 0, 64)
	for cursor := ptrValue; ; cursor++ {
		value := *(*byte)(unsafe.Pointer(cursor))
		if value == 0 {
			break
		}
		data = append(data, value)
	}
	return string(data)
}

func ptr[T any](value *T) uintptr {
	if value == nil {
		return 0
	}
	return uintptr(unsafe.Pointer(value))
}

func locateDLL(explicitPath string) (string, error) {
	candidates := make([]string, 0, 8)
	seen := map[string]struct{}{}
	add := func(path string) {
		if path == "" {
			return
		}
		clean := filepath.Clean(path)
		if _, ok := seen[clean]; ok {
			return
		}
		seen[clean] = struct{}{}
		candidates = append(candidates, clean)
	}

	add(explicitPath)
	add(os.Getenv("DXBRIDGE_DLL"))

	if cwd, err := os.Getwd(); err == nil {
		add(filepath.Join(cwd, "dxbridge.dll"))
	}

	if exe, err := os.Executable(); err == nil {
		add(filepath.Join(filepath.Dir(exe), "dxbridge.dll"))
	}

	_, sourceFile, _, ok := runtime.Caller(0)
	if ok {
		exampleRoot := findMarkerParent(filepath.Dir(sourceFile), "go.mod")
		if exampleRoot != "" {
			add(filepath.Join(exampleRoot, "dxbridge.dll"))
		}
		repoRoot := findRepoRoot(filepath.Dir(sourceFile))
		if repoRoot != "" {
			add(filepath.Join(repoRoot, "out", "build", "debug", "Debug", "dxbridge.dll"))
			add(filepath.Join(repoRoot, "out", "build", "debug", "examples", "Debug", "dxbridge.dll"))
			add(filepath.Join(repoRoot, "out", "build", "debug", "tests", "Debug", "dxbridge.dll"))
			add(filepath.Join(repoRoot, "out", "build", "ci", "Release", "dxbridge.dll"))
		}
	}

	for _, candidate := range candidates {
		resolved, err := filepath.Abs(candidate)
		if err != nil {
			continue
		}
		if info, statErr := os.Stat(resolved); statErr == nil && !info.IsDir() {
			return resolved, nil
		}
	}

	return "", fmt.Errorf("could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\nSearched:\n  - %s", strings.Join(candidates, "\n  - "))
}

func findRepoRoot(start string) string {
	return findMarkerParent(start, filepath.Join("include", "dxbridge", "dxbridge.h"))
}

func findMarkerParent(start string, marker string) string {
	current := start
	for {
		if _, err := os.Stat(filepath.Join(current, marker)); err == nil {
			return current
		}
		next := filepath.Dir(current)
		if next == current {
			return ""
		}
		current = next
	}
}
