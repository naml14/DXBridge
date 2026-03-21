#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{
    DxBridge, DxbDevice, DxbRtv, DxbSwapChain, DXB_BACKEND_DX11, DXB_DEVICE_FLAG_DEBUG,
    DXB_FORMAT_RGBA8_UNORM, DXB_NULL_HANDLE,
};
use dxbridge_rust_examples::win32::Win32Window;

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example04_dx11_clear_window [--dll PATH] [--width 640] [--height 360] [--frames 180] [--color R,G,B,A] [--hidden] [--debug]");
        return Ok(());
    }

    let width = cli.get_u32("--width", 640)?;
    let height = cli.get_u32("--height", 360)?;
    let frames = cli.get_u32("--frames", 180)?;
    let clear_color = parse_rgba(&cli.get_string("--color", "0.08,0.16,0.32,1.0"))?;

    let mut window = Win32Window::new(
        "DXBridge Rust Clear Window",
        width as i32,
        height as i32,
        !cli.has_flag("--hidden"),
        "DXBridgeRustClearWindow",
    )?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;

    let mut initialized = false;
    let mut device: DxbDevice = DXB_NULL_HANDLE;
    let mut swap_chain: DxbSwapChain = DXB_NULL_HANDLE;
    let mut rtv: DxbRtv = DXB_NULL_HANDLE;

    let result = (|| -> AnyResult<()> {
        let hwnd = window.create()?;

        println!("Loaded: {}", bridge.path.display());
        println!("Window: {}", window.title());
        println!("Created HWND: 0x{:016X}", hwnd as usize);

        bridge.init(DXB_BACKEND_DX11)?;
        initialized = true;

        let flags = if cli.has_flag("--debug") {
            DXB_DEVICE_FLAG_DEBUG
        } else {
            0
        };
        device = bridge.create_device(DXB_BACKEND_DX11, 0, flags)?;
        swap_chain = bridge.create_swap_chain(
            device,
            hwnd as *mut _,
            width,
            height,
            DXB_FORMAT_RGBA8_UNORM,
            2,
            0,
        )?;
        let back_buffer = bridge.get_back_buffer(swap_chain)?;
        rtv = bridge.create_render_target_view(device, back_buffer)?;

        println!("Rendering frames. Close the window manually or wait for auto-close.");
        for frame in 0..frames {
            if !window.pump_messages() {
                break;
            }
            bridge.begin_frame(device)?;
            bridge.set_render_targets(device, rtv, DXB_NULL_HANDLE)?;
            bridge.clear_render_target(rtv, clear_color)?;
            bridge.end_frame(device)?;
            bridge.present(swap_chain, 1)?;

            if frame + 1 == frames {
                window.close();
            }
        }

        println!("Done.");
        Ok(())
    })();

    if rtv != DXB_NULL_HANDLE {
        bridge.destroy_render_target_view(rtv);
    }
    if swap_chain != DXB_NULL_HANDLE {
        bridge.destroy_swap_chain(swap_chain);
    }
    if device != DXB_NULL_HANDLE {
        bridge.destroy_device(device);
    }
    if initialized {
        bridge.shutdown();
    }

    result
}

fn parse_rgba(value: &str) -> AnyResult<[f32; 4]> {
    let parts = value.split(',').map(|part| part.trim()).collect::<Vec<_>>();
    if parts.len() != 4 {
        return Err("--color must be R,G,B,A".into());
    }
    let mut rgba = [0.0f32; 4];
    for (index, part) in parts.iter().enumerate() {
        rgba[index] = part.parse::<f32>()?;
    }
    Ok(rgba)
}
