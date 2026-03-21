using System.Runtime.InteropServices;

namespace DXBridgeExamples.DxBridge;

internal sealed class Win32Window : IDisposable
{
    private const uint CsVRedraw = 0x0001;
    private const uint CsHRedraw = 0x0002;
    private const uint WsOverlappedWindow = 0x00CF0000;
    private const int CwUseDefault = unchecked((int)0x80000000);
    private const int SwHide = 0;
    private const int SwShow = 5;
    private const uint PmRemove = 0x0001;
    private const uint WmDestroy = 0x0002;
    private const uint WmClose = 0x0010;
    private const uint WmQuit = 0x0012;
    private const int IdcArrow = 32512;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WndClassEx
    {
        public uint CbSize;
        public uint Style;
        public IntPtr LpfnWndProc;
        public int CbClsExtra;
        public int CbWndExtra;
        public IntPtr HInstance;
        public IntPtr HIcon;
        public IntPtr HCursor;
        public IntPtr HbrBackground;
        public string? LpszMenuName;
        public string LpszClassName;
        public IntPtr HIconSm;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X;
        public int Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Msg
    {
        public IntPtr Hwnd;
        public uint Message;
        public IntPtr WParam;
        public IntPtr LParam;
        public uint Time;
        public Point Pt;
        public uint LPrivate;
    }

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate IntPtr WindowProc(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr GetModuleHandleW(string? moduleName);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadCursorW(IntPtr hInstance, IntPtr cursorName);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern ushort RegisterClassExW(ref WndClassEx lpwcx);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateWindowExW(
        uint exStyle,
        string className,
        string windowName,
        uint style,
        int x,
        int y,
        int width,
        int height,
        IntPtr parent,
        IntPtr menu,
        IntPtr instance,
        IntPtr param);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hwnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern bool UpdateWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool PeekMessageW(out Msg lpMsg, IntPtr hwnd, uint minFilter, uint maxFilter, uint removeMsg);

    [DllImport("user32.dll")]
    private static extern bool TranslateMessage(in Msg lpMsg);

    [DllImport("user32.dll")]
    private static extern IntPtr DispatchMessageW(in Msg lpMsg);

    [DllImport("user32.dll")]
    private static extern IntPtr DefWindowProcW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool DestroyWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern void PostQuitMessage(int exitCode);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool UnregisterClassW(string className, IntPtr instance);

    private readonly IntPtr _instanceHandle;
    private readonly string _className;
    private readonly WindowProc _windowProc;
    private bool _classRegistered;
    private bool _closed;

    public Win32Window(string title, int width, int height, bool visible, string classPrefix)
    {
        Title = title;
        Width = width;
        Height = height;
        Visible = visible;
        _instanceHandle = GetModuleHandleW(null);
        _className = $"{classPrefix}_{Environment.TickCount64:X}";
        _windowProc = HandleMessage;
    }

    public string Title { get; }
    public int Width { get; }
    public int Height { get; }
    public bool Visible { get; }
    public IntPtr Handle { get; private set; }
    public bool IsClosed => _closed;

    public IntPtr Create()
    {
        var windowClass = new WndClassEx
        {
            CbSize = (uint)Marshal.SizeOf<WndClassEx>(),
            Style = CsHRedraw | CsVRedraw,
            LpfnWndProc = Marshal.GetFunctionPointerForDelegate(_windowProc),
            HInstance = _instanceHandle,
            HCursor = LoadCursorW(IntPtr.Zero, new IntPtr(IdcArrow)),
            LpszClassName = _className,
        };

        if (RegisterClassExW(ref windowClass) == 0)
        {
            throw new InvalidOperationException("RegisterClassExW failed.");
        }

        _classRegistered = true;
        Handle = CreateWindowExW(0, _className, Title, WsOverlappedWindow, CwUseDefault, CwUseDefault, Width, Height, IntPtr.Zero, IntPtr.Zero, _instanceHandle, IntPtr.Zero);
        if (Handle == IntPtr.Zero)
        {
            Destroy();
            throw new InvalidOperationException("CreateWindowExW failed.");
        }

        ShowWindow(Handle, Visible ? SwShow : SwHide);
        UpdateWindow(Handle);
        return Handle;
    }

    public bool PumpMessages()
    {
        while (PeekMessageW(out var message, IntPtr.Zero, 0, 0, PmRemove))
        {
            TranslateMessage(message);
            DispatchMessageW(message);
            if (message.Message == WmQuit)
            {
                _closed = true;
            }
        }

        return !_closed;
    }

    public void Destroy()
    {
        if (Handle != IntPtr.Zero)
        {
            DestroyWindow(Handle);
            Handle = IntPtr.Zero;
        }

        if (_classRegistered)
        {
            UnregisterClassW(_className, _instanceHandle);
            _classRegistered = false;
        }
    }

    public void Dispose()
    {
        Destroy();
    }

    private IntPtr HandleMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam)
    {
        if (message == WmClose)
        {
            DestroyWindow(hwnd);
            return IntPtr.Zero;
        }

        if (message == WmDestroy)
        {
            _closed = true;
            if (Handle == hwnd)
            {
                Handle = IntPtr.Zero;
            }

            PostQuitMessage(0);
            return IntPtr.Zero;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
