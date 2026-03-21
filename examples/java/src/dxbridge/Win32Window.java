package dxbridge;

import com.sun.jna.Pointer;
import com.sun.jna.platform.win32.Kernel32;
import com.sun.jna.platform.win32.User32;
import com.sun.jna.platform.win32.WinDef.ATOM;
import com.sun.jna.platform.win32.WinDef.HCURSOR;
import com.sun.jna.platform.win32.WinDef.HINSTANCE;
import com.sun.jna.platform.win32.WinDef.HWND;
import com.sun.jna.platform.win32.WinDef.LRESULT;
import com.sun.jna.platform.win32.WinDef.WPARAM;
import com.sun.jna.platform.win32.WinDef.LPARAM;
import com.sun.jna.platform.win32.WinNT.HANDLE;
import com.sun.jna.platform.win32.WinUser;

public final class Win32Window {
    private static final int CS_HREDRAW = 0x0002;
    private static final int CS_VREDRAW = 0x0001;
    private static final int CW_USEDEFAULT = 0x80000000;
    private static final int PM_REMOVE = 0x0001;

    private final String title;
    private final int width;
    private final int height;
    private final boolean visible;
    private final HINSTANCE instanceHandle;
    private final String className;
    private final WinUser.WindowProc windowProc;

    private HWND hwnd;
    private boolean closed;
    private boolean classRegistered;

    public Win32Window(String title, int width, int height, boolean visible, String classPrefix) {
        this.title = title;
        this.width = width;
        this.height = height;
        this.visible = visible;
        this.instanceHandle = Kernel32.INSTANCE.GetModuleHandle(null);
        this.className = classPrefix + "_" + Long.toHexString(System.nanoTime());
        this.windowProc = new WinUser.WindowProc() {
            @Override
            public LRESULT callback(HWND windowHandle, int message, WPARAM wParam, LPARAM lParam) {
                return handleMessage(windowHandle, message, wParam, lParam);
            }
        };
    }

    public HWND create() {
        WinUser.WNDCLASSEX windowClass = new WinUser.WNDCLASSEX();
        windowClass.cbSize = windowClass.size();
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance = instanceHandle;
        windowClass.hCursor = loadArrowCursor();
        windowClass.lpszClassName = className;
        windowClass.write();

        ATOM atom = User32.INSTANCE.RegisterClassEx(windowClass);
        if (atom == null || atom.intValue() == 0) {
            throw new IllegalStateException("RegisterClassEx failed");
        }
        classRegistered = true;

        hwnd = User32.INSTANCE.CreateWindowEx(
            0,
            className,
            title,
            WinUser.WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            null,
            null,
            instanceHandle,
            null
        );
        if (hwnd == null) {
            destroy();
            throw new IllegalStateException("CreateWindowEx failed");
        }

        User32.INSTANCE.ShowWindow(hwnd, visible ? WinUser.SW_SHOW : WinUser.SW_HIDE);
        User32.INSTANCE.UpdateWindow(hwnd);
        return hwnd;
    }

    public boolean pumpMessages() {
        WinUser.MSG message = new WinUser.MSG();
        while (User32.INSTANCE.PeekMessage(message, null, 0, 0, PM_REMOVE)) {
            User32.INSTANCE.TranslateMessage(message);
            User32.INSTANCE.DispatchMessage(message);
            if (message.message == WinUser.WM_QUIT) {
                closed = true;
            }
        }
        return !closed;
    }

    public void destroy() {
        if (hwnd != null) {
            User32.INSTANCE.DestroyWindow(hwnd);
            hwnd = null;
        }
        if (classRegistered) {
            User32.INSTANCE.UnregisterClass(className, instanceHandle);
            classRegistered = false;
        }
    }

    public Pointer getHwndPointer() {
        return hwnd == null ? null : hwnd.getPointer();
    }

    public long getHwndValue() {
        return hwnd == null ? 0L : Pointer.nativeValue(hwnd.getPointer());
    }

    public boolean isClosed() {
        return closed;
    }

    private LRESULT handleMessage(HWND windowHandle, int message, WPARAM wParam, LPARAM lParam) {
        if (message == WinUser.WM_CLOSE) {
            User32.INSTANCE.DestroyWindow(windowHandle);
            return new LRESULT(0);
        }

        if (message == WinUser.WM_DESTROY) {
            closed = true;
            if (hwnd != null && hwnd.equals(windowHandle)) {
                hwnd = null;
            }
            User32.INSTANCE.PostQuitMessage(0);
            return new LRESULT(0);
        }

        return User32.INSTANCE.DefWindowProc(windowHandle, message, wParam, lParam);
    }

    private static HCURSOR loadArrowCursor() {
        HANDLE cursorHandle = User32.INSTANCE.LoadImage(
            null,
            Integer.toString(WinUser.IDC_ARROW),
            WinUser.IMAGE_CURSOR,
            0,
            0,
            WinUser.LR_SHARED
        );
        if (cursorHandle == null) {
            return null;
        }
        return new HCURSOR(cursorHandle.getPointer());
    }
}
