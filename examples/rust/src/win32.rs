#![cfg(windows)]

use std::error::Error;
use std::ffi::c_void;
use std::ptr::null_mut;

use windows_sys::Win32::Foundation::{GetLastError, HINSTANCE, HWND, LPARAM, LRESULT, WPARAM};
use windows_sys::Win32::System::LibraryLoader::GetModuleHandleW;
use windows_sys::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DestroyWindow, DispatchMessageW, GetWindowLongPtrW,
    LoadCursorW, PeekMessageW, PostQuitMessage, RegisterClassExW, SetWindowLongPtrW, ShowWindow,
    TranslateMessage, UnregisterClassW, CREATESTRUCTW, CW_USEDEFAULT, GWLP_USERDATA, IDC_ARROW,
    MSG, PM_REMOVE, SW_HIDE, SW_SHOW, WM_CLOSE, WM_DESTROY, WM_NCCREATE, WM_QUIT, WNDCLASSEXW,
    WS_OVERLAPPEDWINDOW,
};

pub type AnyResult<T> = Result<T, Box<dyn Error>>;

pub struct Win32Window {
    title: String,
    width: i32,
    height: i32,
    visible: bool,
    instance: HINSTANCE,
    class_name: Vec<u16>,
    title_wide: Vec<u16>,
    hwnd: HWND,
    closed: bool,
    class_registered: bool,
}

impl Win32Window {
    pub fn new(
        title: &str,
        width: i32,
        height: i32,
        visible: bool,
        class_prefix: &str,
    ) -> AnyResult<Self> {
        let instance = unsafe { GetModuleHandleW(std::ptr::null()) };
        if instance.is_null() {
            return Err(format!("GetModuleHandleW failed: {}", unsafe { GetLastError() }).into());
        }

        let class_name = to_wide(&format!("{}_{}", class_prefix, std::process::id()));
        let title_wide = to_wide(title);

        Ok(Self {
            title: title.to_string(),
            width,
            height,
            visible,
            instance,
            class_name,
            title_wide,
            hwnd: null_mut(),
            closed: false,
            class_registered: false,
        })
    }

    pub fn create(&mut self) -> AnyResult<HWND> {
        let cursor = unsafe { LoadCursorW(null_mut(), IDC_ARROW) };
        let class = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            style: 0x0001 | 0x0002,
            lpfnWndProc: Some(window_proc),
            cbClsExtra: 0,
            cbWndExtra: 0,
            hInstance: self.instance,
            hIcon: null_mut(),
            hCursor: cursor,
            hbrBackground: null_mut(),
            lpszMenuName: std::ptr::null(),
            lpszClassName: self.class_name.as_ptr(),
            hIconSm: null_mut(),
        };

        let atom = unsafe { RegisterClassExW(&class) };
        if atom == 0 {
            return Err(format!("RegisterClassExW failed: {}", unsafe { GetLastError() }).into());
        }
        self.class_registered = true;

        let hwnd = unsafe {
            CreateWindowExW(
                0,
                self.class_name.as_ptr(),
                self.title_wide.as_ptr(),
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                self.width,
                self.height,
                null_mut(),
                null_mut(),
                self.instance,
                self as *mut Self as *mut c_void,
            )
        };

        if hwnd.is_null() {
            self.destroy();
            return Err(format!("CreateWindowExW failed: {}", unsafe { GetLastError() }).into());
        }

        self.hwnd = hwnd;
        unsafe {
            ShowWindow(hwnd, if self.visible { SW_SHOW } else { SW_HIDE });
        }
        Ok(hwnd)
    }

    pub fn handle(&self) -> HWND {
        self.hwnd
    }

    pub fn title(&self) -> &str {
        &self.title
    }

    pub fn pump_messages(&mut self) -> bool {
        let mut message: MSG = unsafe { std::mem::zeroed() };
        loop {
            let has_message = unsafe { PeekMessageW(&mut message, null_mut(), 0, 0, PM_REMOVE) };
            if has_message == 0 {
                break;
            }
            unsafe {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if message.message == WM_QUIT {
                self.closed = true;
            }
        }
        !self.closed
    }

    pub fn close(&mut self) {
        if !self.hwnd.is_null() {
            unsafe {
                DestroyWindow(self.hwnd);
            }
            self.hwnd = null_mut();
        }
    }

    pub fn destroy(&mut self) {
        if !self.hwnd.is_null() {
            unsafe {
                DestroyWindow(self.hwnd);
            }
            self.hwnd = null_mut();
        }
        if self.class_registered {
            unsafe {
                UnregisterClassW(self.class_name.as_ptr(), self.instance);
            }
            self.class_registered = false;
        }
    }
}

impl Drop for Win32Window {
    fn drop(&mut self) {
        self.destroy();
    }
}

unsafe extern "system" fn window_proc(
    hwnd: HWND,
    message: u32,
    w_param: WPARAM,
    l_param: LPARAM,
) -> LRESULT {
    if message == WM_NCCREATE {
        let create_struct = l_param as *const CREATESTRUCTW;
        if !create_struct.is_null() {
            let window = (*create_struct).lpCreateParams as *mut Win32Window;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, window as isize);
        }
    }

    let window_ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA) as *mut Win32Window;
    if !window_ptr.is_null() {
        let window = &mut *window_ptr;
        match message {
            WM_CLOSE => {
                DestroyWindow(hwnd);
                return 0;
            }
            WM_DESTROY => {
                window.closed = true;
                if window.hwnd == hwnd {
                    window.hwnd = null_mut();
                }
                PostQuitMessage(0);
                return 0;
            }
            _ => {}
        }
    }

    DefWindowProcW(hwnd, message, w_param, l_param)
}

fn to_wide(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(std::iter::once(0)).collect()
}
