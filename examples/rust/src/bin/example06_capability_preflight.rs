#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{
    format_feature_level, parse_backend, DxBridge, DXB_BACKEND_DX11, DXB_BACKEND_DX12,
    DXB_FEATURE_DX12,
};

const BACKENDS: &[(u32, &str, &str)] = &[
    (DXB_BACKEND_DX11, "dx11", "DX11"),
    (DXB_BACKEND_DX12, "dx12", "DX12"),
];

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example06_capability_preflight [--dll PATH] [--backend all|dx11|dx12] [--compare-active dx11|dx12]");
        return Ok(());
    }

    let selected_backends = select_backends(&cli.get_string("--backend", "all"))?;
    let compare_backend =
        select_compare_backend(cli.get_optional_string("--compare-active").as_deref())?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;
    println!("Loaded: {}", bridge.path.display());
    println!("Scenario 06 - Capability preflight");
    println!("Purpose: inspect backend readiness before DXBridge_Init().");
    println!("Backends: {}", join_backend_labels(&selected_backends));
    println!("Pre-init capability discovery:");
    for (backend, _, label) in &selected_backends {
        print_backend(&bridge, *backend, label)?;
    }
    if let Some((backend, _, label)) = compare_backend {
        print_legacy_comparison(&bridge, backend, label)?;
    } else {
        println!("Tip: pass --compare-active dx11 or --compare-active dx12 to contrast the legacy active-backend check.");
    }
    Ok(())
}

fn select_backends(name: &str) -> AnyResult<Vec<(u32, &'static str, &'static str)>> {
    if name.eq_ignore_ascii_case("all") || name.trim().is_empty() {
        return Ok(BACKENDS.to_vec());
    }

    let backend = parse_backend(name)?;
    BACKENDS
        .iter()
        .copied()
        .find(|candidate| candidate.0 == backend)
        .map(|candidate| vec![candidate])
        .ok_or_else(|| format!("unsupported backend for capability preflight: {}", name).into())
}

fn select_compare_backend(
    name: Option<&str>,
) -> AnyResult<Option<(u32, &'static str, &'static str)>> {
    let Some(value) = name.filter(|value| !value.trim().is_empty()) else {
        return Ok(None);
    };

    let backend = parse_backend(value)?;
    if backend == 0 {
        return Err("--compare-active must be dx11 or dx12.".into());
    }

    Ok(BACKENDS
        .iter()
        .copied()
        .find(|candidate| candidate.0 == backend))
}

fn join_backend_labels(backends: &[(u32, &'static str, &'static str)]) -> String {
    backends
        .iter()
        .map(|candidate| candidate.2)
        .collect::<Vec<_>>()
        .join(", ")
}

fn print_legacy_comparison(bridge: &DxBridge, backend: u32, label: &str) -> AnyResult<()> {
    println!("Legacy comparison after DXBridge_Init():");
    bridge.init(backend)?;
    println!("  active backend: {}", label);
    println!(
        "  DXBridge_SupportsFeature(DXB_FEATURE_DX12): {}",
        bridge.supports_feature(DXB_FEATURE_DX12)
    );
    println!("  This remains an active-backend check, not a machine-wide preflight query.");
    bridge.shutdown();
    Ok(())
}

fn print_backend(bridge: &DxBridge, backend: u32, label: &str) -> AnyResult<()> {
    let available = bridge.query_backend_available(backend)?;
    let debug = bridge.query_debug_layer_available(backend)?;
    let gpu = bridge.query_gpu_validation_available(backend)?;
    let count = bridge.query_adapter_count(backend)?;

    println!("{}:", label);
    println!(
        "  available: {} (best FL {})",
        available.supported != 0,
        format_feature_level(available.value_u32)
    );
    println!(
        "  debug layer: {} (best FL {})",
        debug.supported != 0,
        format_feature_level(debug.value_u32)
    );
    println!(
        "  GPU validation: {} (best FL {})",
        gpu.supported != 0,
        format_feature_level(gpu.value_u32)
    );
    println!("  adapter count: {}", count.value_u32);

    for adapter_index in 0..count.value_u32 {
        let software = bridge.query_adapter_software(backend, adapter_index)?;
        let feature_level = bridge.query_adapter_max_feature_level(backend, adapter_index)?;
        println!(
            "  adapter {}: software={} maxFL={}",
            adapter_index,
            software.value_u32 != 0,
            format_feature_level(feature_level.value_u32)
        );
    }

    Ok(())
}
