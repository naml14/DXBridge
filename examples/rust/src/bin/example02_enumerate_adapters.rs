#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{decode_description, format_bytes, parse_backend, DxBridge};

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example02_enumerate_adapters [--dll PATH] [--backend auto|dx11|dx12]");
        return Ok(());
    }

    let backend = parse_backend(&cli.get_string("--backend", "auto"))?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;

    println!("Loaded: {}", bridge.path.display());
    bridge.init(backend)?;
    let adapters = bridge.enumerate_adapters()?;
    bridge.shutdown();

    if adapters.is_empty() {
        println!("No adapters reported by DXBridge.");
        return Ok(());
    }

    println!("Found {} adapter(s):", adapters.len());
    for adapter in adapters {
        let kind = if adapter.is_software != 0 {
            "software"
        } else {
            "hardware"
        };
        println!(
            "- #{}: {} [{}] | VRAM={} | Shared={}",
            adapter.index,
            decode_description(&adapter.description),
            kind,
            format_bytes(adapter.dedicated_video_mem),
            format_bytes(adapter.shared_system_mem),
        );
    }

    Ok(())
}
