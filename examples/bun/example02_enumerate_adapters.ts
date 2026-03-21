import {
  DXBridgeLibrary,
  formatBytes,
  getStringArg,
  parseArgs,
  parseBackend,
} from "./dxbridge";

function printUsage(): void {
  console.log("Usage: bun run examples/bun/example02_enumerate_adapters.ts [--dll PATH] [--backend auto|dx11|dx12]");
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

  try {
    dxbridge.init(backend);
    const adapters = dxbridge.enumerateAdapters();
    if (adapters.length === 0) {
      console.log("No adapters reported by DXBridge.");
      return;
    }

    console.log(`Found ${adapters.length} adapter(s):`);
    for (const adapter of adapters) {
      const kind = adapter.isSoftware ? "software" : "hardware";
      console.log(
        `- #${adapter.index}: ${adapter.description} [${kind}] | VRAM=${formatBytes(adapter.dedicatedVideoMem)} | Shared=${formatBytes(adapter.sharedSystemMem)}`,
      );
    }
  } finally {
    dxbridge.shutdown();
    dxbridge.close();
  }
}

main();
