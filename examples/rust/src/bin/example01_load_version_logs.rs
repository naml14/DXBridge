#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{parse_backend, parse_version, DxBridge, DXB_FEATURE_DX12};

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example01_load_version_logs [--dll PATH] [--backend auto|dx11|dx12]");
        return Ok(());
    }

    let backend = parse_backend(&cli.get_string("--backend", "auto"))?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;

    println!("Loaded: {}", bridge.path.display());
    bridge.set_log_callback(|level, message| {
        let label = match level {
            1 => "WARN",
            2 => "ERROR",
            _ => "INFO",
        };
        println!("[log:{}] {}", label, message);
    });

    let version = bridge.get_version();
    println!(
        "DXBridge version: {} (0x{:08X})",
        parse_version(version),
        version
    );
    bridge.init(backend)?;

    println!(
        "DX12 feature flag reported by active backend: {}",
        bridge.supports_feature(DXB_FEATURE_DX12)
    );
    bridge.shutdown();
    bridge.clear_log_callback();
    println!("Initialization and shutdown completed successfully.");
    Ok(())
}
