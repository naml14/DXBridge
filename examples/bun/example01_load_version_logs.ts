import {
  DXB_BACKEND_AUTO,
  DXB_FEATURE_DX12,
  DXB_LOG_LEVEL_NAMES,
  DXBridgeLibrary,
  getStringArg,
  parseArgs,
  parseBackend,
  parseVersion,
} from "./dxbridge";

function printUsage(): void {
  console.log("Usage: bun run examples/bun/example01_load_version_logs.ts [--dll PATH] [--backend auto|dx11|dx12]");
}

function main(): void {
  const args = parseArgs(Bun.argv.slice(2));
  if (args.values.help === true) {
    printUsage();
    return;
  }

  const backend = parseBackend(getStringArg(args, "backend", "auto"));
  const dllPath = getStringArg(args, "dll");
  const dxbridge = new DXBridgeLibrary(dllPath || undefined);

  console.log(`Loaded: ${dxbridge.path}`);
  dxbridge.setLogCallback((level, message) => {
    const label = DXB_LOG_LEVEL_NAMES[level] ?? `LEVEL_${level}`;
    console.log(`[log:${label}] ${message}`);
  });

  try {
    const version = dxbridge.getVersion();
    console.log(`DXBridge version: ${parseVersion(version)} (0x${version.toString(16).toUpperCase().padStart(8, "0")})`);

    dxbridge.init(Number.isFinite(backend) ? backend : DXB_BACKEND_AUTO);
    console.log(`DX12 feature flag reported by active backend: ${dxbridge.supportsFeature(DXB_FEATURE_DX12)}`);
    console.log("Initialization and shutdown completed successfully.");
    dxbridge.shutdown();
  } finally {
    dxbridge.close();
  }
}

main();
