#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{
    format_hresult, parse_backend, DxBridge, DxbDevice, DXB_DEVICE_FLAG_DEBUG,
};

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example03_create_device_errors [--dll PATH] [--backend auto|dx11|dx12] [--debug]");
        return Ok(());
    }

    let backend = parse_backend(&cli.get_string("--backend", "dx11"))?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;

    println!("Loaded: {}", bridge.path.display());
    println!("1) Intentionally failing CreateDevice before DXBridge_Init...");
    let mut not_initialized_device: DxbDevice = 0;
    let result = unsafe { bridge.create_device_raw(std::ptr::null(), &mut not_initialized_device) };
    println!("   result={} | {}", result, bridge.describe_failure(result));
    println!("   Read GetLastError immediately: the buffer is thread-local and later calls can overwrite it.");

    println!("2) Initializing DXBridge...");
    bridge.init(backend)?;

    println!("3) Creating a real device...");
    let flags = if cli.has_flag("--debug") {
        DXB_DEVICE_FLAG_DEBUG
    } else {
        0
    };
    let device = bridge.create_device(backend, 0, flags)?;
    println!("   device handle: {}", device);
    println!(
        "   feature level: {}",
        format_hresult(bridge.get_feature_level(device))
    );
    println!("   Device creation succeeded.");

    bridge.destroy_device(device);
    bridge.shutdown();
    Ok(())
}
