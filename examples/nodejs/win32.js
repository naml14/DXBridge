const koffi = require('koffi');

const CS_VREDRAW = 0x0001;
const CS_HREDRAW = 0x0002;
const WS_OVERLAPPEDWINDOW = 0x00cf0000;
const CW_USEDEFAULT = -2147483648;
const SW_HIDE = 0;
const SW_SHOW = 5;
const PM_REMOVE = 0x0001;
const WM_DESTROY = 0x0002;
const WM_CLOSE = 0x0010;
const WM_QUIT = 0x0012;
const IDC_ARROW = 32512;
const WNDCLASSEXW_SIZE = 80;

const WndProc = koffi.proto('intptr_t __stdcall WndProc(void *hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam)');

const WNDCLASSEXW = koffi.struct('WNDCLASSEXW', {
  cbSize: 'uint32_t',
  style: 'uint32_t',
  lpfnWndProc: koffi.pointer(WndProc),
  cbClsExtra: 'int32_t',
  cbWndExtra: 'int32_t',
  hInstance: 'void *',
  hIcon: 'void *',
  hCursor: 'void *',
  hbrBackground: 'void *',
  lpszMenuName: 'const char16_t *',
  lpszClassName: 'const char16_t *',
  hIconSm: 'void *'
});

const POINT = koffi.struct('POINT', {
  x: 'int32_t',
  y: 'int32_t'
});

const MSG = koffi.struct('MSG', {
  hwnd: 'void *',
  message: 'uint32_t',
  wParam: 'uintptr_t',
  lParam: 'intptr_t',
  time: 'uint32_t',
  pt: POINT,
  lPrivate: 'uint32_t'
});

const kernel32 = koffi.load('kernel32.dll');
const user32 = koffi.load('user32.dll');

const GetModuleHandleW = kernel32.func('void * __stdcall GetModuleHandleW(const char16_t *lpModuleName)');
const LoadCursorW = user32.func('void * __stdcall LoadCursorW(void *hInstance, intptr_t lpCursorName)');
const RegisterClassExW = user32.func('uint16_t __stdcall RegisterClassExW(const WNDCLASSEXW *wndclass)');
const CreateWindowExW = user32.func('void * __stdcall CreateWindowExW(uint32_t exStyle, const char16_t *className, const char16_t *windowName, uint32_t style, int32_t x, int32_t y, int32_t width, int32_t height, void *parent, void *menu, void *instance, void *param)');
const ShowWindow = user32.func('bool __stdcall ShowWindow(void *hwnd, int32_t cmdShow)');
const UpdateWindow = user32.func('bool __stdcall UpdateWindow(void *hwnd)');
const PeekMessageW = user32.func('bool __stdcall PeekMessageW(_Out_ MSG *lpMsg, void *hwnd, uint32_t minFilter, uint32_t maxFilter, uint32_t removeMsg)');
const TranslateMessage = user32.func('bool __stdcall TranslateMessage(const MSG *lpMsg)');
const DispatchMessageW = user32.func('intptr_t __stdcall DispatchMessageW(const MSG *lpMsg)');
const DefWindowProcW = user32.func('intptr_t __stdcall DefWindowProcW(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam)');
const DestroyWindow = user32.func('bool __stdcall DestroyWindow(void *hwnd)');
const PostQuitMessage = user32.func('void __stdcall PostQuitMessage(int32_t exitCode)');
const UnregisterClassW = user32.func('bool __stdcall UnregisterClassW(const char16_t *className, void *instance)');

class Win32Window {
  constructor(title, width, height, visible, classPrefix) {
    this.title = title;
    this.width = width;
    this.height = height;
    this.visible = visible;
    this.className = `${classPrefix}_${process.pid}`;
    this.instance = GetModuleHandleW(null);
    this.hwnd = null;
    this.hwndValue = 0n;
    this.closed = false;
    this.classRegistered = false;

    this.wndProcPointer = koffi.register((hwnd, message, wParam, lParam) => this.#windowProc(hwnd, message, wParam, lParam), koffi.pointer(WndProc));
  }

  create() {
    const cursor = LoadCursorW(null, IDC_ARROW);
    const klass = {
      cbSize: WNDCLASSEXW_SIZE,
      style: CS_HREDRAW | CS_VREDRAW,
      lpfnWndProc: this.wndProcPointer,
      cbClsExtra: 0,
      cbWndExtra: 0,
      hInstance: this.instance,
      hIcon: null,
      hCursor: cursor,
      hbrBackground: null,
      lpszMenuName: null,
      lpszClassName: this.className,
      hIconSm: null
    };

    const atom = RegisterClassExW(klass);
    if (atom === 0) {
      throw new Error('RegisterClassExW failed.');
    }
    this.classRegistered = true;

    const hwnd = CreateWindowExW(
      0,
      this.className,
      this.title,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      this.width,
      this.height,
      null,
      null,
      this.instance,
      null
    );

    if (!hwnd) {
      this.destroy();
      throw new Error('CreateWindowExW failed.');
    }

    this.hwnd = hwnd;
    this.hwndValue = koffi.address(hwnd);
    ShowWindow(hwnd, this.visible ? SW_SHOW : SW_HIDE);
    UpdateWindow(hwnd);
    return this.hwndValue;
  }

  pumpMessages() {
    const message = {};
    while (PeekMessageW(message, null, 0, 0, PM_REMOVE)) {
      TranslateMessage(message);
      DispatchMessageW(message);
      if (message.message === WM_QUIT) {
        this.closed = true;
      }
    }
    return !this.closed;
  }

  close() {
    if (this.hwnd) {
      DestroyWindow(this.hwnd);
    }
  }

  destroy() {
    if (this.hwnd) {
      DestroyWindow(this.hwnd);
      this.hwnd = null;
      this.hwndValue = 0n;
    }
    if (this.classRegistered) {
      UnregisterClassW(this.className, this.instance);
      this.classRegistered = false;
    }
  }

  dispose() {
    this.destroy();
    if (this.wndProcPointer) {
      koffi.unregister(this.wndProcPointer);
      this.wndProcPointer = null;
    }
  }

  #windowProc(hwnd, message, wParam, lParam) {
    if (message === WM_CLOSE) {
      DestroyWindow(hwnd);
      return 0;
    }

    if (message === WM_DESTROY) {
      this.closed = true;
      if (this.hwnd && koffi.address(this.hwnd) === koffi.address(hwnd)) {
        this.hwnd = null;
        this.hwndValue = 0n;
      }
      PostQuitMessage(0);
      return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
  }
}

module.exports = {
  Win32Window
};
