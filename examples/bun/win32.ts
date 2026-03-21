import { JSCallback, dlopen, ptr } from "bun:ffi";

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
const MSG_SIZE = 48;

const kernel32 = dlopen("kernel32.dll", {
  GetModuleHandleW: { args: ["ptr"], returns: "ptr" },
});

const user32 = dlopen("user32.dll", {
  LoadCursorW: { args: ["ptr", "ptr"], returns: "ptr" },
  RegisterClassExW: { args: ["ptr"], returns: "u16" },
  CreateWindowExW: {
    args: ["u32", "ptr", "ptr", "u32", "i32", "i32", "i32", "i32", "ptr", "ptr", "ptr", "ptr"],
    returns: "ptr",
  },
  ShowWindow: { args: ["ptr", "i32"], returns: "bool" },
  UpdateWindow: { args: ["ptr"], returns: "bool" },
  PeekMessageW: { args: ["ptr", "ptr", "u32", "u32", "u32"], returns: "bool" },
  TranslateMessage: { args: ["ptr"], returns: "bool" },
  DispatchMessageW: { args: ["ptr"], returns: "ptr" },
  DefWindowProcW: { args: ["ptr", "u32", "ptr", "ptr"], returns: "ptr" },
  DestroyWindow: { args: ["ptr"], returns: "bool" },
  PostQuitMessage: { args: ["i32"], returns: "void" },
  UnregisterClassW: { args: ["ptr", "ptr"], returns: "bool" },
});

function createWideString(text: string): Uint16Array {
  const data = new Uint16Array(text.length + 1);
  for (let index = 0; index < text.length; index += 1) {
    data[index] = text.charCodeAt(index);
  }
  data[text.length] = 0;
  return data;
}

function writeU32(view: DataView, offset: number, value: number): void {
  view.setUint32(offset, value >>> 0, true);
}

function writeI32(view: DataView, offset: number, value: number): void {
  view.setInt32(offset, value | 0, true);
}

function writePtr(view: DataView, offset: number, value: number | bigint): void {
  view.setBigUint64(offset, typeof value === "bigint" ? value : BigInt(value), true);
}

export class Win32Window {
  readonly title: string;
  readonly width: number;
  readonly height: number;
  readonly visible: boolean;
  readonly className: string;

  #instance = 0;
  #classNameUTF16: Uint16Array;
  #wndProc: JSCallback;
  #classRegistered = false;
  #closed = false;

  hwnd = 0;

  constructor(title: string, width: number, height: number, visible: boolean, classPrefix: string) {
    this.title = title;
    this.width = width;
    this.height = height;
    this.visible = visible;
    this.className = `${classPrefix}_${process.pid}`;
    this.#classNameUTF16 = createWideString(this.className);
    this.#instance = kernel32.symbols.GetModuleHandleW(0);
    this.#wndProc = new JSCallback(
      (hwnd: number, message: number, wParam: number, lParam: number) => this.#windowProc(hwnd, message, wParam, lParam),
      { returns: "ptr", args: ["ptr", "u32", "ptr", "ptr"] },
    );
  }

  create(): number {
    const titleUTF16 = createWideString(this.title);
    const klass = new Uint8Array(WNDCLASSEXW_SIZE);
    const view = new DataView(klass.buffer, klass.byteOffset, klass.byteLength);
    const cursor = user32.symbols.LoadCursorW(0, IDC_ARROW);

    writeU32(view, 0, WNDCLASSEXW_SIZE);
    writeU32(view, 4, CS_HREDRAW | CS_VREDRAW);
    writePtr(view, 8, this.#wndProc.ptr);
    writeI32(view, 16, 0);
    writeI32(view, 20, 0);
    writePtr(view, 24, this.#instance);
    writePtr(view, 32, 0);
    writePtr(view, 40, cursor);
    writePtr(view, 48, 0);
    writePtr(view, 56, 0);
    writePtr(view, 64, this.#pointerOf(this.#classNameUTF16));
    writePtr(view, 72, 0);

    const atom = user32.symbols.RegisterClassExW(klass);
    if (atom === 0) {
      throw new Error("RegisterClassExW failed.");
    }
    this.#classRegistered = true;

    const hwnd = user32.symbols.CreateWindowExW(
      0,
      this.#classNameUTF16,
      titleUTF16,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      this.width,
      this.height,
      0,
      0,
      this.#instance,
      0,
    );
    if (hwnd === 0) {
      this.destroy();
      throw new Error("CreateWindowExW failed.");
    }

    this.hwnd = hwnd;
    user32.symbols.ShowWindow(hwnd, this.visible ? SW_SHOW : SW_HIDE);
    user32.symbols.UpdateWindow(hwnd);
    return hwnd;
  }

  pumpMessages(): boolean {
    const message = new Uint8Array(MSG_SIZE);
    const view = new DataView(message.buffer, message.byteOffset, message.byteLength);
    while (user32.symbols.PeekMessageW(message, 0, 0, 0, PM_REMOVE)) {
      user32.symbols.TranslateMessage(message);
      user32.symbols.DispatchMessageW(message);
      if (view.getUint32(8, true) === WM_QUIT) {
        this.#closed = true;
      }
    }
    return !this.#closed;
  }

  close(): void {
    if (this.hwnd !== 0) {
      user32.symbols.DestroyWindow(this.hwnd);
    }
  }

  destroy(): void {
    if (this.hwnd !== 0) {
      user32.symbols.DestroyWindow(this.hwnd);
      this.hwnd = 0;
    }
    if (this.#classRegistered) {
      user32.symbols.UnregisterClassW(this.#classNameUTF16, this.#instance);
      this.#classRegistered = false;
    }
  }

  dispose(): void {
    this.destroy();
    this.#wndProc.close();
  }

  #windowProc(hwnd: number, message: number, wParam: number, lParam: number): number {
    if (message === WM_CLOSE) {
      user32.symbols.DestroyWindow(hwnd);
      return 0;
    }

    if (message === WM_DESTROY) {
      this.#closed = true;
      if (this.hwnd === hwnd) {
        this.hwnd = 0;
      }
      user32.symbols.PostQuitMessage(0);
      return 0;
    }

    return user32.symbols.DefWindowProcW(hwnd, message, wParam, lParam);
  }

  #pointerOf(value: Uint16Array): number {
    return ptr(value);
  }
}
