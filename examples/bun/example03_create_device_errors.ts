import {
  DXB_DEVICE_FLAG_DEBUG,
  DXBRIDGE_NULL_HANDLE,
  DXBridgeLibrary,
  formatHRESULT,
  getBooleanArg,
  getStringArg,
  parseArgs,
  parseBackend,
} from "./dxbridge";

function printUsage(): void {
  console.log("Usage: bun run examples/bun/example03_create_device_errors.ts [--dll PATH] [--backend auto|dx11|dx12] [--debug]");
}

function main(): void {
  const args = parseArgs(Bun.argv.slice(2));
  if (args.values.help === true) {
    printUsage();
    return;
  }

  const backend = parseBackend(getStringArg(args, "backend", "dx11"));
  const debug = getBooleanArg(args, "debug");
  const dllPath = getStringArg(args, "dll");
  const dxbridge = new DXBridgeLibrary(dllPath || undefined);

  console.log(`Loaded: ${dxbridge.path}`);
  console.log("1) Intentionally failing CreateDevice before DXBridge_Init...");
  const notInitializedDevice = new BigUint64Array(1);
  const result = dxbridge.createDeviceCall(null, notInitializedDevice);
  console.log(`   result=${result} | ${dxbridge.describeFailure(result)}`);
  console.log("   Read GetLastError immediately: the buffer is thread-local and later calls can overwrite it.");

  let device = DXBRIDGE_NULL_HANDLE;
  try {
    console.log("2) Initializing DXBridge...");
    dxbridge.init(backend);

    console.log("3) Creating a real device...");
    device = dxbridge.createDevice({
      backend,
      adapterIndex: 0,
      flags: debug ? DXB_DEVICE_FLAG_DEBUG : 0,
    });

    console.log(`   device handle: ${device.toString()}`);
    console.log(`   feature level: ${formatHRESULT(dxbridge.getFeatureLevel(device))}`);
    console.log("   Device creation succeeded.");
  } finally {
    if (device !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyDevice(device);
    }
    dxbridge.shutdown();
    dxbridge.close();
  }
}

main();
