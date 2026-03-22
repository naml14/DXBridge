const {
  DXB_FEATURE_DX12,
  DXB_BACKEND_DX11,
  DXB_BACKEND_DX12,
  DXBridgeError,
  DXBridgeLibrary,
  formatFeatureLevel,
  getStringArg,
  parseArgs,
  parseBackend
} = require('./dxbridge');

const BACKENDS = [
  { id: DXB_BACKEND_DX11, name: 'DX11' },
  { id: DXB_BACKEND_DX12, name: 'DX12' }
];

function printUsage() {
  console.log('Usage: node examples/nodejs/example06_capability_preflight.js [--dll PATH] [--backend all|dx11|dx12] [--compare-active dx11|dx12]');
}

function selectBackends(name) {
  if (!name || String(name).toLowerCase() === 'all') {
    return BACKENDS;
  }

  const backendId = parseBackend(name);
  const backend = BACKENDS.find(candidate => candidate.id === backendId);
  if (!backend) {
    throw new DXBridgeError(`Unsupported backend ${JSON.stringify(name)} for capability preflight.`);
  }
  return [backend];
}

function parseCompareBackend(name) {
  if (!name) {
    return null;
  }
  const backendId = parseBackend(name);
  if (backendId === 0) {
    throw new DXBridgeError('--compare-active must be dx11 or dx12.');
  }
  return BACKENDS.find(candidate => candidate.id === backendId) || null;
}

function printLegacyComparison(dxbridge, backend) {
  console.log('Legacy comparison after DXBridge_Init():');
  dxbridge.init(backend.id);
  try {
    console.log(`  active backend: ${backend.name}`);
    console.log(`  DXBridge_SupportsFeature(DXB_FEATURE_DX12): ${dxbridge.supportsFeature(DXB_FEATURE_DX12)}`);
    console.log('  This remains an active-backend check, not a machine-wide preflight query.');
  } finally {
    dxbridge.shutdown();
  }
}

function printBackend(dxbridge, backend) {
  const available = dxbridge.queryBackendAvailable(backend.id);
  const debug = dxbridge.queryDebugLayerAvailable(backend.id);
  const gpu = dxbridge.queryGPUValidationAvailable(backend.id);
  const count = dxbridge.queryAdapterCount(backend.id);

  console.log(`${backend.name}:`);
  console.log(`  available: ${available.supported} (best FL ${formatFeatureLevel(available.valueU32)})`);
  console.log(`  debug layer: ${debug.supported} (best FL ${formatFeatureLevel(debug.valueU32)})`);
  console.log(`  GPU validation: ${gpu.supported} (best FL ${formatFeatureLevel(gpu.valueU32)})`);
  console.log(`  adapter count: ${count.valueU32}`);

  for (let index = 0; index < count.valueU32; index += 1) {
    const software = dxbridge.queryAdapterSoftware(backend.id, index);
    const featureLevel = dxbridge.queryAdapterMaxFeatureLevel(backend.id, index);
    console.log(
      `  adapter ${index}: software=${software.valueU32 !== 0} maxFL=${formatFeatureLevel(featureLevel.valueU32)}`
    );
  }
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.values.help === true) {
    printUsage();
    return;
  }

  const selectedBackends = selectBackends(getStringArg(args, 'backend', 'all'));
  const compareBackend = parseCompareBackend(getStringArg(args, 'compare-active'));
  const dxbridge = new DXBridgeLibrary(getStringArg(args, 'dll') || undefined);
  try {
    console.log(`Loaded: ${dxbridge.path}`);
    console.log('Scenario 06 - Capability preflight');
    console.log('Purpose: inspect backend readiness before DXBridge_Init().');
    console.log(`Backends: ${selectedBackends.map(backend => backend.name).join(', ')}`);
    console.log('Pre-init capability discovery:');
    for (const backend of selectedBackends) {
      printBackend(dxbridge, backend);
    }
    if (compareBackend) {
      printLegacyComparison(dxbridge, compareBackend);
    } else {
      console.log('Tip: pass --compare-active dx11 or --compare-active dx12 to contrast the legacy active-backend check.');
    }
  } finally {
    dxbridge.close();
  }
}

main();
