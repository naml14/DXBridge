//go:build windows

package dxbridge

import (
	"fmt"
	"syscall"
	"unsafe"
)

const (
	csVRedraw          = 0x0001
	csHRedraw          = 0x0002
	wsOverlappedWindow = 0x00CF0000
	cwUseDefault       = 0x80000000
	swHide             = 0
	swShow             = 5
	pmRemove           = 0x0001
	wmDestroy          = 0x0002
	wmClose            = 0x0010
	wmQuit             = 0x0012
	idcArrow           = 32512
)

var (
	user32               = syscall.NewLazyDLL("user32.dll")
	kernel32             = syscall.NewLazyDLL("kernel32.dll")
	procGetModuleHandleW = kernel32.NewProc("GetModuleHandleW")
	procLoadCursorW      = user32.NewProc("LoadCursorW")
	procRegisterClassExW = user32.NewProc("RegisterClassExW")
	procCreateWindowExW  = user32.NewProc("CreateWindowExW")
	procShowWindow       = user32.NewProc("ShowWindow")
	procUpdateWindow     = user32.NewProc("UpdateWindow")
	procPeekMessageW     = user32.NewProc("PeekMessageW")
	procTranslateMessage = user32.NewProc("TranslateMessage")
	procDispatchMessageW = user32.NewProc("DispatchMessageW")
	procDefWindowProcW   = user32.NewProc("DefWindowProcW")
	procDestroyWindow    = user32.NewProc("DestroyWindow")
	procPostQuitMessage  = user32.NewProc("PostQuitMessage")
	procUnregisterClassW = user32.NewProc("UnregisterClassW")
)

type wndClassEx struct {
	Size       uint32
	Style      uint32
	WndProc    uintptr
	ClsExtra   int32
	WndExtra   int32
	Instance   uintptr
	Icon       uintptr
	Cursor     uintptr
	Background uintptr
	MenuName   *uint16
	ClassName  *uint16
	IconSmall  uintptr
}

type point struct {
	X int32
	Y int32
}

type msg struct {
	HWND    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      point
	Private uint32
}

type Win32Window struct {
	title           string
	width           int
	height          int
	visible         bool
	instance        uintptr
	className       string
	classNameUTF16  *uint16
	hwnd            uintptr
	closed          bool
	classRegistered bool
	callback        uintptr
}

func NewWin32Window(title string, width int, height int, visible bool, classPrefix string) (*Win32Window, error) {
	instance, _, _ := procGetModuleHandleW.Call(0)
	className := fmt.Sprintf("%s_%d", classPrefix, syscall.Getpid())
	classNameUTF16, err := syscall.UTF16PtrFromString(className)
	if err != nil {
		return nil, err
	}
	window := &Win32Window{
		title:          title,
		width:          width,
		height:         height,
		visible:        visible,
		instance:       instance,
		className:      className,
		classNameUTF16: classNameUTF16,
	}
	window.callback = syscall.NewCallback(window.windowProc)
	return window, nil
}

func (w *Win32Window) Create() (uintptr, error) {
	titleUTF16, err := syscall.UTF16PtrFromString(w.title)
	if err != nil {
		return 0, err
	}

	cursor, _, _ := procLoadCursorW.Call(0, uintptr(idcArrow))
	klass := wndClassEx{
		Size:      uint32(unsafe.Sizeof(wndClassEx{})),
		Style:     csHRedraw | csVRedraw,
		WndProc:   w.callback,
		Instance:  w.instance,
		Cursor:    cursor,
		ClassName: w.classNameUTF16,
	}
	r1, _, _ := procRegisterClassExW.Call(uintptr(unsafe.Pointer(&klass)))
	if r1 == 0 {
		return 0, fmt.Errorf("RegisterClassExW failed: %w", syscall.GetLastError())
	}
	w.classRegistered = true

	hwnd, _, _ := procCreateWindowExW.Call(
		0,
		uintptr(unsafe.Pointer(w.classNameUTF16)),
		uintptr(unsafe.Pointer(titleUTF16)),
		uintptr(wsOverlappedWindow),
		uintptr(cwUseDefault),
		uintptr(cwUseDefault),
		uintptr(w.width),
		uintptr(w.height),
		0,
		0,
		w.instance,
		0,
	)
	if hwnd == 0 {
		w.Destroy()
		return 0, fmt.Errorf("CreateWindowExW failed: %w", syscall.GetLastError())
	}

	w.hwnd = hwnd
	show := uintptr(swHide)
	if w.visible {
		show = uintptr(swShow)
	}
	procShowWindow.Call(hwnd, show)
	procUpdateWindow.Call(hwnd)
	return hwnd, nil
}

func (w *Win32Window) Handle() uintptr {
	return w.hwnd
}

func (w *Win32Window) PumpMessages() bool {
	var message msg
	for {
		r1, _, _ := procPeekMessageW.Call(uintptr(unsafe.Pointer(&message)), 0, 0, 0, pmRemove)
		if r1 == 0 {
			break
		}
		procTranslateMessage.Call(uintptr(unsafe.Pointer(&message)))
		procDispatchMessageW.Call(uintptr(unsafe.Pointer(&message)))
		if message.Message == wmQuit {
			w.closed = true
		}
	}
	return !w.closed
}

func (w *Win32Window) Close() {
	if w.hwnd != 0 {
		procDestroyWindow.Call(w.hwnd)
	}
}

func (w *Win32Window) Destroy() {
	if w.hwnd != 0 {
		procDestroyWindow.Call(w.hwnd)
		w.hwnd = 0
	}
	if w.classRegistered {
		procUnregisterClassW.Call(uintptr(unsafe.Pointer(w.classNameUTF16)), w.instance)
		w.classRegistered = false
	}
}

func (w *Win32Window) windowProc(hwnd uintptr, message uint32, wParam uintptr, lParam uintptr) uintptr {
	switch message {
	case wmClose:
		procDestroyWindow.Call(hwnd)
		return 0
	case wmDestroy:
		w.closed = true
		if w.hwnd == hwnd {
			w.hwnd = 0
		}
		procPostQuitMessage.Call(0)
		return 0
	default:
		r1, _, _ := procDefWindowProcW.Call(hwnd, uintptr(message), wParam, lParam)
		return r1
	}
}
