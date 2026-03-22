const fs = require('node:fs');
const path = require('node:path');
const koffi = require('koffi');

const DXBRIDGE_NULL_HANDLE = 0n;
const DXBRIDGE_STRUCT_VERSION = 1;

const DXB_OK = 0;
const DXB_E_INVALID_HANDLE = -1;
const DXB_E_DEVICE_LOST = -2;
const DXB_E_OUT_OF_MEMORY = -3;
const DXB_E_INVALID_ARG = -4;
const DXB_E_NOT_SUPPORTED = -5;
const DXB_E_SHADER_COMPILE = -6;
const DXB_E_NOT_INITIALIZED = -7;
const DXB_E_INVALID_STATE = -8;
const DXB_E_INTERNAL = -999;

const DXB_BACKEND_AUTO = 0;
const DXB_BACKEND_DX11 = 1;
const DXB_BACKEND_DX12 = 2;

const DXB_FORMAT_UNKNOWN = 0;
const DXB_FORMAT_RGBA8_UNORM = 1;
const DXB_FORMAT_RGBA16_FLOAT = 2;
const DXB_FORMAT_D32_FLOAT = 3;
const DXB_FORMAT_D24_UNORM_S8 = 4;
const DXB_FORMAT_R32_UINT = 5;
const DXB_FORMAT_RG32_FLOAT = 6;
const DXB_FORMAT_RGB32_FLOAT = 7;
const DXB_FORMAT_RGBA32_FLOAT = 8;

const DXB_DEVICE_FLAG_DEBUG = 0x0001;
const DXB_DEVICE_FLAG_GPU_VALIDATION = 0x0002;

const DXB_COMPILE_DEBUG = 0x0001;

const DXB_BUFFER_FLAG_VERTEX = 0x0001;
const DXB_BUFFER_FLAG_INDEX = 0x0002;
const DXB_BUFFER_FLAG_CONSTANT = 0x0004;
const DXB_BUFFER_FLAG_DYNAMIC = 0x0008;

const DXB_SHADER_STAGE_VERTEX = 0;
const DXB_SHADER_STAGE_PIXEL = 1;

const DXB_PRIM_TRIANGLELIST = 0;
const DXB_PRIM_TRIANGLESTRIP = 1;
const DXB_PRIM_LINELIST = 2;
const DXB_PRIM_POINTLIST = 3;

const DXB_SEMANTIC_POSITION = 0;
const DXB_SEMANTIC_NORMAL = 1;
const DXB_SEMANTIC_TEXCOORD = 2;
const DXB_SEMANTIC_COLOR = 3;

const DXB_FEATURE_DX12 = 0x0001;

const DXB_RESULT_NAMES = {
  [DXB_OK]: 'DXB_OK',
  [DXB_E_INVALID_HANDLE]: 'DXB_E_INVALID_HANDLE',
  [DXB_E_DEVICE_LOST]: 'DXB_E_DEVICE_LOST',
  [DXB_E_OUT_OF_MEMORY]: 'DXB_E_OUT_OF_MEMORY',
  [DXB_E_INVALID_ARG]: 'DXB_E_INVALID_ARG',
  [DXB_E_NOT_SUPPORTED]: 'DXB_E_NOT_SUPPORTED',
  [DXB_E_SHADER_COMPILE]: 'DXB_E_SHADER_COMPILE',
  [DXB_E_NOT_INITIALIZED]: 'DXB_E_NOT_INITIALIZED',
  [DXB_E_INVALID_STATE]: 'DXB_E_INVALID_STATE',
  [DXB_E_INTERNAL]: 'DXB_E_INTERNAL'
};

const DXB_LOG_LEVEL_NAMES = {
  0: 'INFO',
  1: 'WARN',
  2: 'ERROR'
};

const DXB_BACKEND_NAMES = {
  auto: DXB_BACKEND_AUTO,
  dx11: DXB_BACKEND_DX11,
  dx12: DXB_BACKEND_DX12
};

const DXB_ADAPTER_INFO_SIZE = 168;
const DXB_DEVICE_DESC_SIZE = 16;
const DXB_SWAPCHAIN_DESC_SIZE = 40;
const DXB_BUFFER_DESC_SIZE = 16;
const DXB_SHADER_DESC_SIZE = 56;
const DXB_INPUT_ELEMENT_DESC_SIZE = 24;
const DXB_PIPELINE_DESC_SIZE = 56;
const DXB_VIEWPORT_SIZE = 24;

const DXBLogCallback = koffi.proto('void DXBLogCallback(int level, const char *msg, void *user_data)');
const DXBShaderDesc = koffi.struct('DXBShaderDesc', {
  struct_version: 'uint32_t',
  stage: 'uint32_t',
  source_code: 'const char *',
  source_size: 'uint32_t',
  source_name: 'const char *',
  entry_point: 'const char *',
  target: 'const char *',
  compile_flags: 'uint32_t'
});

class DXBridgeError extends Error {
  constructor(message) {
    super(message);
    this.name = 'DXBridgeError';
  }
}

function assertWindows() {
  if (process.platform !== 'win32') {
    throw new DXBridgeError('These Node.js examples require Windows because dxbridge.dll is a Windows DLL.');
  }
}

function formatSearched(candidates) {
  return candidates.map(candidate => `  - ${candidate}`).join('\n');
}

function parseVersion(version) {
  const major = (version >>> 16) & 0xff;
  const minor = (version >>> 8) & 0xff;
  const patch = version & 0xff;
  return `${major}.${minor}.${patch}`;
}

function formatHRESULT(value) {
  return `0x${(value >>> 0).toString(16).toUpperCase().padStart(8, '0')}`;
}

function formatBytes(value) {
  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
  let amount = Number(value);
  let unit = units[0];
  for (const candidate of units) {
    unit = candidate;
    if (amount < 1024 || candidate === units[units.length - 1]) {
      break;
    }
    amount /= 1024;
  }
  if (unit === 'B') {
    return `${Math.trunc(amount)} ${unit}`;
  }
  return `${amount.toFixed(1)} ${unit}`;
}

function parseBackend(name) {
  const backend = DXB_BACKEND_NAMES[String(name).toLowerCase()];
  if (backend === undefined) {
    throw new DXBridgeError(`Unknown backend ${JSON.stringify(name)}. Use auto, dx11, or dx12.`);
  }
  return backend;
}

function parseArgs(argv) {
  const positional = [];
  const values = {};

  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];
    if (!token.startsWith('--')) {
      positional.push(token);
      continue;
    }

    const body = token.slice(2);
    const equalsIndex = body.indexOf('=');
    if (equalsIndex >= 0) {
      values[body.slice(0, equalsIndex)] = body.slice(equalsIndex + 1);
      continue;
    }

    const next = argv[index + 1];
    if (next && !next.startsWith('--')) {
      values[body] = next;
      index += 1;
      continue;
    }

    values[body] = true;
  }

  return { positional, values };
}

function getStringArg(args, key, fallback = '') {
  const value = args.values[key];
  return typeof value === 'string' ? value : fallback;
}

function getBooleanArg(args, key) {
  return args.values[key] === true;
}

function decodeCString(buffer, offset = 0, length = buffer.length - offset) {
  const end = buffer.indexOf(0, offset);
  const safeEnd = end >= 0 && end < offset + length ? end : offset + length;
  return buffer.toString('utf8', offset, safeEnd);
}

function writeU32(buffer, offset, value) {
  buffer.writeUInt32LE(value >>> 0, offset);
}

function writeU64(buffer, offset, value) {
  buffer.writeBigUInt64LE(BigInt(value), offset);
}

function writeF32(buffer, offset, value) {
  buffer.writeFloatLE(value, offset);
}

function writePtr(buffer, offset, value) {
  let address = 0n;
  if (value === null || value === undefined) {
    address = 0n;
  } else if (typeof value === 'bigint' || typeof value === 'number') {
    address = BigInt(value);
  } else {
    address = koffi.address(value);
  }
  buffer.writeBigUInt64LE(address, offset);
}

function encodeDeviceDesc(desc) {
  const buffer = Buffer.alloc(DXB_DEVICE_DESC_SIZE);
  writeU32(buffer, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(buffer, 4, desc.backend);
  writeU32(buffer, 8, desc.adapterIndex ?? 0);
  writeU32(buffer, 12, desc.flags ?? 0);
  return buffer;
}

function encodeSwapChainDesc(desc) {
  const buffer = Buffer.alloc(DXB_SWAPCHAIN_DESC_SIZE);
  writeU32(buffer, 0, DXBRIDGE_STRUCT_VERSION);
  writePtr(buffer, 8, desc.hwnd);
  writeU32(buffer, 16, desc.width);
  writeU32(buffer, 20, desc.height);
  writeU32(buffer, 24, desc.format ?? DXB_FORMAT_RGBA8_UNORM);
  writeU32(buffer, 28, desc.bufferCount ?? 2);
  writeU32(buffer, 32, desc.flags ?? 0);
  return buffer;
}

function encodeBufferDesc(desc) {
  const buffer = Buffer.alloc(DXB_BUFFER_DESC_SIZE);
  writeU32(buffer, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(buffer, 4, desc.flags);
  writeU32(buffer, 8, desc.byteSize);
  writeU32(buffer, 12, desc.stride ?? 0);
  return buffer;
}

function encodeShaderDesc(desc) {
  return {
    struct_version: DXBRIDGE_STRUCT_VERSION,
    stage: desc.stage,
    source_code: desc.sourceCode,
    source_size: 0,
    source_name: desc.sourceName ?? 'shader',
    entry_point: desc.entryPoint ?? 'main',
    target: desc.target,
    compile_flags: desc.compileFlags ?? 0
  };
}

function encodeInputElements(elements) {
  const buffer = Buffer.alloc(elements.length * DXB_INPUT_ELEMENT_DESC_SIZE);
  for (let index = 0; index < elements.length; index += 1) {
    const base = index * DXB_INPUT_ELEMENT_DESC_SIZE;
    const element = elements[index];
    writeU32(buffer, base + 0, DXBRIDGE_STRUCT_VERSION);
    writeU32(buffer, base + 4, element.semantic);
    writeU32(buffer, base + 8, element.semanticIndex ?? 0);
    writeU32(buffer, base + 12, element.format);
    writeU32(buffer, base + 16, element.inputSlot ?? 0);
    writeU32(buffer, base + 20, element.byteOffset);
  }
  return buffer;
}

function encodePipelineDesc(desc) {
  const buffer = Buffer.alloc(DXB_PIPELINE_DESC_SIZE);
  writeU32(buffer, 0, DXBRIDGE_STRUCT_VERSION);
  writeU64(buffer, 8, desc.vs);
  writeU64(buffer, 16, desc.ps);
  writeU64(buffer, 24, desc.inputLayout);
  writeU32(buffer, 32, desc.topology ?? DXB_PRIM_TRIANGLELIST);
  writeU32(buffer, 36, desc.depthTest ?? 0);
  writeU32(buffer, 40, desc.depthWrite ?? 0);
  writeU32(buffer, 44, desc.cullBack ?? 0);
  writeU32(buffer, 48, desc.wireframe ?? 0);
  return buffer;
}

function encodeViewport(viewport) {
  const buffer = Buffer.alloc(DXB_VIEWPORT_SIZE);
  writeF32(buffer, 0, viewport.x ?? 0);
  writeF32(buffer, 4, viewport.y ?? 0);
  writeF32(buffer, 8, viewport.width);
  writeF32(buffer, 12, viewport.height);
  writeF32(buffer, 16, viewport.minDepth ?? 0);
  writeF32(buffer, 20, viewport.maxDepth ?? 1);
  return buffer;
}

function toByteBuffer(data) {
  if (Buffer.isBuffer(data)) {
    return data;
  }
  if (ArrayBuffer.isView(data)) {
    return Buffer.from(data.buffer, data.byteOffset, data.byteLength);
  }
  if (data instanceof ArrayBuffer) {
    return Buffer.from(data);
  }
  throw new DXBridgeError('Expected a Buffer, TypedArray, DataView, or ArrayBuffer.');
}

function locateDXBridgeDLL(explicitPath) {
  assertWindows();

  const here = __dirname;
  const repoRoot = path.resolve(here, '..', '..');
  const candidates = [
    explicitPath,
    process.env.DXBRIDGE_DLL,
    path.join(process.cwd(), 'dxbridge.dll'),
    path.join(here, 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'debug', 'Debug', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'debug', 'examples', 'Debug', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'debug', 'tests', 'Debug', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'release', 'Release', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'release', 'examples', 'Release', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'release', 'tests', 'Release', 'dxbridge.dll'),
    path.join(repoRoot, 'out', 'build', 'ci', 'Release', 'dxbridge.dll')
  ].filter(Boolean);

  for (const candidate of candidates) {
    const resolved = path.resolve(candidate);
    if (fs.existsSync(resolved)) {
      return resolved;
    }
  }

  throw new DXBridgeError(
    `Could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\nSearched:\n${formatSearched(candidates)}`
  );
}

class DXBridgeLibrary {
  constructor(explicitPath) {
    this.path = locateDXBridgeDLL(explicitPath);
    this.library = koffi.load(this.path);
    this.raw = {
      DXBridge_SetLogCallback: this.library.func('void DXBridge_SetLogCallback(DXBLogCallback *cb, void *user_data)'),
      DXBridge_Init: this.library.func('int32_t DXBridge_Init(uint32_t backend_hint)'),
      DXBridge_Shutdown: this.library.func('void DXBridge_Shutdown(void)'),
      DXBridge_GetVersion: this.library.func('uint32_t DXBridge_GetVersion(void)'),
      DXBridge_SupportsFeature: this.library.func('uint32_t DXBridge_SupportsFeature(uint32_t feature_flag)'),
      DXBridge_GetLastError: this.library.func('void DXBridge_GetLastError(_Out_ uint8_t *buf, int buf_size)'),
      DXBridge_GetLastHRESULT: this.library.func('uint32_t DXBridge_GetLastHRESULT(void)'),
      DXBridge_EnumerateAdapters: this.library.func('int32_t DXBridge_EnumerateAdapters(void *out_buf, _Inout_ uint32_t *in_out_count)'),
      DXBridge_CreateDevice: this.library.func('int32_t DXBridge_CreateDevice(const void *desc, _Out_ uint64_t *out_device)'),
      DXBridge_DestroyDevice: this.library.func('void DXBridge_DestroyDevice(uint64_t device)'),
      DXBridge_GetFeatureLevel: this.library.func('uint32_t DXBridge_GetFeatureLevel(uint64_t device)'),
      DXBridge_CreateSwapChain: this.library.func('int32_t DXBridge_CreateSwapChain(uint64_t device, const void *desc, _Out_ uint64_t *out_sc)'),
      DXBridge_DestroySwapChain: this.library.func('void DXBridge_DestroySwapChain(uint64_t sc)'),
      DXBridge_ResizeSwapChain: this.library.func('int32_t DXBridge_ResizeSwapChain(uint64_t sc, uint32_t width, uint32_t height)'),
      DXBridge_GetBackBuffer: this.library.func('int32_t DXBridge_GetBackBuffer(uint64_t sc, _Out_ uint64_t *out_rt)'),
      DXBridge_CreateRenderTargetView: this.library.func('int32_t DXBridge_CreateRenderTargetView(uint64_t device, uint64_t rt, _Out_ uint64_t *out_rtv)'),
      DXBridge_DestroyRTV: this.library.func('void DXBridge_DestroyRTV(uint64_t rtv)'),
      DXBridge_CreateDepthStencilView: this.library.func('int32_t DXBridge_CreateDepthStencilView(uint64_t device, const void *desc, _Out_ uint64_t *out_dsv)'),
      DXBridge_DestroyDSV: this.library.func('void DXBridge_DestroyDSV(uint64_t dsv)'),
      DXBridge_CompileShader: this.library.func('int32_t DXBridge_CompileShader(uint64_t device, const DXBShaderDesc *desc, _Out_ uint64_t *out_shader)'),
      DXBridge_DestroyShader: this.library.func('void DXBridge_DestroyShader(uint64_t shader)'),
      DXBridge_CreateInputLayout: this.library.func('int32_t DXBridge_CreateInputLayout(uint64_t device, const void *elements, uint32_t element_count, uint64_t vs, _Out_ uint64_t *out_layout)'),
      DXBridge_DestroyInputLayout: this.library.func('void DXBridge_DestroyInputLayout(uint64_t layout)'),
      DXBridge_CreatePipelineState: this.library.func('int32_t DXBridge_CreatePipelineState(uint64_t device, const void *desc, _Out_ uint64_t *out_pipeline)'),
      DXBridge_DestroyPipeline: this.library.func('void DXBridge_DestroyPipeline(uint64_t pipeline)'),
      DXBridge_CreateBuffer: this.library.func('int32_t DXBridge_CreateBuffer(uint64_t device, const void *desc, _Out_ uint64_t *out_buf)'),
      DXBridge_UploadData: this.library.func('int32_t DXBridge_UploadData(uint64_t buf, const void *data, uint32_t byte_size)'),
      DXBridge_DestroyBuffer: this.library.func('void DXBridge_DestroyBuffer(uint64_t buf)'),
      DXBridge_BeginFrame: this.library.func('int32_t DXBridge_BeginFrame(uint64_t device)'),
      DXBridge_SetRenderTargets: this.library.func('int32_t DXBridge_SetRenderTargets(uint64_t device, uint64_t rtv, uint64_t dsv)'),
      DXBridge_ClearRenderTarget: this.library.func('int32_t DXBridge_ClearRenderTarget(uint64_t rtv, const float *rgba)'),
      DXBridge_ClearDepthStencil: this.library.func('int32_t DXBridge_ClearDepthStencil(uint64_t dsv, float depth, uint8_t stencil)'),
      DXBridge_SetViewport: this.library.func('int32_t DXBridge_SetViewport(uint64_t device, const void *vp)'),
      DXBridge_SetPipeline: this.library.func('int32_t DXBridge_SetPipeline(uint64_t device, uint64_t pipeline)'),
      DXBridge_SetVertexBuffer: this.library.func('int32_t DXBridge_SetVertexBuffer(uint64_t device, uint64_t buf, uint32_t stride, uint32_t offset)'),
      DXBridge_SetIndexBuffer: this.library.func('int32_t DXBridge_SetIndexBuffer(uint64_t device, uint64_t buf, uint32_t fmt, uint32_t offset)'),
      DXBridge_SetConstantBuffer: this.library.func('int32_t DXBridge_SetConstantBuffer(uint64_t device, uint32_t slot, uint64_t buf)'),
      DXBridge_Draw: this.library.func('int32_t DXBridge_Draw(uint64_t device, uint32_t vertex_count, uint32_t start_vertex)'),
      DXBridge_DrawIndexed: this.library.func('int32_t DXBridge_DrawIndexed(uint64_t device, uint32_t index_count, uint32_t start_index, int32_t base_vertex)'),
      DXBridge_EndFrame: this.library.func('int32_t DXBridge_EndFrame(uint64_t device)'),
      DXBridge_Present: this.library.func('int32_t DXBridge_Present(uint64_t sc, uint32_t sync_interval)'),
      DXBridge_GetNativeDevice: this.library.func('void *DXBridge_GetNativeDevice(uint64_t device)'),
      DXBridge_GetNativeContext: this.library.func('void *DXBridge_GetNativeContext(uint64_t device)')
    };

    this.logCallbackPointer = null;
  }

  close() {
    this.setLogCallback(null);
  }

  setLogCallback(handler) {
    if (this.logCallbackPointer) {
      this.raw.DXBridge_SetLogCallback(null, null);
      koffi.unregister(this.logCallbackPointer);
      this.logCallbackPointer = null;
    }

    if (!handler) {
      return;
    }

    this.logCallbackPointer = koffi.register((level, message, userData) => {
      handler(level, message || '', userData);
    }, koffi.pointer(DXBLogCallback));

    this.raw.DXBridge_SetLogCallback(this.logCallbackPointer, null);
  }

  getVersion() {
    return this.raw.DXBridge_GetVersion();
  }

  supportsFeature(featureFlag) {
    return this.raw.DXBridge_SupportsFeature(featureFlag) !== 0;
  }

  init(backendHint) {
    this.requireOK(this.raw.DXBridge_Init(backendHint), 'DXBridge_Init');
  }

  shutdown() {
    this.raw.DXBridge_Shutdown();
  }

  getLastError() {
    const buffer = Buffer.alloc(512);
    this.raw.DXBridge_GetLastError(buffer, buffer.length);
    return decodeCString(buffer);
  }

  getLastHRESULT() {
    return this.raw.DXBridge_GetLastHRESULT();
  }

  describeFailure(result) {
    const name = DXB_RESULT_NAMES[result] || `DXBResult(${result})`;
    const message = this.getLastError();
    const hresult = formatHRESULT(this.getLastHRESULT());
    return message ? `${name}: ${message} (HRESULT ${hresult})` : `${name} (HRESULT ${hresult})`;
  }

  requireOK(result, functionName) {
    if (result === DXB_OK) {
      return;
    }
    throw new DXBridgeError(`${functionName} failed: ${this.describeFailure(result)}`);
  }

  enumerateAdapters() {
    const countRef = [0];
    this.requireOK(this.raw.DXBridge_EnumerateAdapters(null, countRef), 'DXBridge_EnumerateAdapters(count)');

    const count = countRef[0] || 0;
    if (count === 0) {
      return [];
    }

    const buffer = Buffer.alloc(count * DXB_ADAPTER_INFO_SIZE);
    for (let index = 0; index < count; index += 1) {
      writeU32(buffer, index * DXB_ADAPTER_INFO_SIZE, DXBRIDGE_STRUCT_VERSION);
    }

    countRef[0] = count;
    this.requireOK(this.raw.DXBridge_EnumerateAdapters(buffer, countRef), 'DXBridge_EnumerateAdapters(fill)');

    const finalCount = countRef[0] || count;
    const adapters = [];
    for (let index = 0; index < finalCount; index += 1) {
      const base = index * DXB_ADAPTER_INFO_SIZE;
      adapters.push({
        structVersion: buffer.readUInt32LE(base + 0),
        index: buffer.readUInt32LE(base + 4),
        description: decodeCString(buffer, base + 8, 128),
        dedicatedVideoMem: buffer.readBigUInt64LE(base + 136),
        dedicatedSystemMem: buffer.readBigUInt64LE(base + 144),
        sharedSystemMem: buffer.readBigUInt64LE(base + 152),
        isSoftware: buffer.readUInt32LE(base + 160) !== 0
      });
    }
    return adapters;
  }

  createDeviceCall(descBuffer, outDevice = [0n]) {
    return this.raw.DXBridge_CreateDevice(descBuffer || null, outDevice);
  }

  createDevice(desc) {
    const descBuffer = encodeDeviceDesc(desc);
    const outDevice = [0n];
    this.requireOK(this.createDeviceCall(descBuffer, outDevice), 'DXBridge_CreateDevice');
    return outDevice[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyDevice(device) {
    this.raw.DXBridge_DestroyDevice(device);
  }

  getFeatureLevel(device) {
    return this.raw.DXBridge_GetFeatureLevel(device);
  }

  createSwapChain(device, desc) {
    const descBuffer = encodeSwapChainDesc(desc);
    const outSwapChain = [0n];
    this.requireOK(this.raw.DXBridge_CreateSwapChain(device, descBuffer, outSwapChain), 'DXBridge_CreateSwapChain');
    return outSwapChain[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroySwapChain(swapChain) {
    this.raw.DXBridge_DestroySwapChain(swapChain);
  }

  resizeSwapChain(swapChain, width, height) {
    this.requireOK(this.raw.DXBridge_ResizeSwapChain(swapChain, width, height), 'DXBridge_ResizeSwapChain');
  }

  getBackBuffer(swapChain) {
    const outRenderTarget = [0n];
    this.requireOK(this.raw.DXBridge_GetBackBuffer(swapChain, outRenderTarget), 'DXBridge_GetBackBuffer');
    return outRenderTarget[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  createRenderTargetView(device, renderTarget) {
    const outRTV = [0n];
    this.requireOK(this.raw.DXBridge_CreateRenderTargetView(device, renderTarget, outRTV), 'DXBridge_CreateRenderTargetView');
    return outRTV[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyRenderTargetView(rtv) {
    this.raw.DXBridge_DestroyRTV(rtv);
  }

  compileShader(device, desc) {
    const encoded = encodeShaderDesc(desc);
    const encodedPtr = koffi.as(encoded, 'DXBShaderDesc *');
    const outShader = [0n];
    this.requireOK(this.raw.DXBridge_CompileShader(device, encodedPtr, outShader), 'DXBridge_CompileShader');
    return outShader[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyShader(shader) {
    this.raw.DXBridge_DestroyShader(shader);
  }

  createInputLayout(device, elements, vertexShader) {
    if (!elements.length) {
      throw new DXBridgeError('createInputLayout requires at least one element.');
    }
    const buffer = encodeInputElements(elements);
    const outLayout = [0n];
    this.requireOK(
      this.raw.DXBridge_CreateInputLayout(device, buffer, elements.length, vertexShader, outLayout),
      'DXBridge_CreateInputLayout'
    );
    return outLayout[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyInputLayout(layout) {
    this.raw.DXBridge_DestroyInputLayout(layout);
  }

  createPipelineState(device, desc) {
    const buffer = encodePipelineDesc(desc);
    const outPipeline = [0n];
    this.requireOK(this.raw.DXBridge_CreatePipelineState(device, buffer, outPipeline), 'DXBridge_CreatePipelineState');
    return outPipeline[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyPipeline(pipeline) {
    this.raw.DXBridge_DestroyPipeline(pipeline);
  }

  createBuffer(device, desc) {
    const bufferDesc = encodeBufferDesc(desc);
    const outBuffer = [0n];
    this.requireOK(this.raw.DXBridge_CreateBuffer(device, bufferDesc, outBuffer), 'DXBridge_CreateBuffer');
    return outBuffer[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  uploadData(bufferHandle, data) {
    const bytes = toByteBuffer(data);
    if (!bytes.byteLength) {
      throw new DXBridgeError('uploadData requires a non-empty typed array or buffer.');
    }
    this.requireOK(this.raw.DXBridge_UploadData(bufferHandle, bytes, bytes.byteLength), 'DXBridge_UploadData');
  }

  destroyBuffer(bufferHandle) {
    this.raw.DXBridge_DestroyBuffer(bufferHandle);
  }

  beginFrame(device) {
    this.requireOK(this.raw.DXBridge_BeginFrame(device), 'DXBridge_BeginFrame');
  }

  setRenderTargets(device, rtv, dsv = DXBRIDGE_NULL_HANDLE) {
    this.requireOK(this.raw.DXBridge_SetRenderTargets(device, rtv, dsv), 'DXBridge_SetRenderTargets');
  }

  clearRenderTarget(rtv, rgba) {
    const color = Float32Array.from(rgba);
    this.requireOK(this.raw.DXBridge_ClearRenderTarget(rtv, color), 'DXBridge_ClearRenderTarget');
  }

  clearDepthStencil(dsv, depth, stencil = 0) {
    this.requireOK(this.raw.DXBridge_ClearDepthStencil(dsv, depth, stencil), 'DXBridge_ClearDepthStencil');
  }

  setViewport(device, viewport) {
    const bytes = encodeViewport(viewport);
    this.requireOK(this.raw.DXBridge_SetViewport(device, bytes), 'DXBridge_SetViewport');
  }

  setPipeline(device, pipeline) {
    this.requireOK(this.raw.DXBridge_SetPipeline(device, pipeline), 'DXBridge_SetPipeline');
  }

  setVertexBuffer(device, bufferHandle, stride, offset = 0) {
    this.requireOK(this.raw.DXBridge_SetVertexBuffer(device, bufferHandle, stride, offset), 'DXBridge_SetVertexBuffer');
  }

  setIndexBuffer(device, bufferHandle, format, offset = 0) {
    this.requireOK(this.raw.DXBridge_SetIndexBuffer(device, bufferHandle, format, offset), 'DXBridge_SetIndexBuffer');
  }

  setConstantBuffer(device, slot, bufferHandle) {
    this.requireOK(this.raw.DXBridge_SetConstantBuffer(device, slot, bufferHandle), 'DXBridge_SetConstantBuffer');
  }

  draw(device, vertexCount, startVertex = 0) {
    this.requireOK(this.raw.DXBridge_Draw(device, vertexCount, startVertex), 'DXBridge_Draw');
  }

  drawIndexed(device, indexCount, startIndex = 0, baseVertex = 0) {
    this.requireOK(this.raw.DXBridge_DrawIndexed(device, indexCount, startIndex, baseVertex), 'DXBridge_DrawIndexed');
  }

  endFrame(device) {
    this.requireOK(this.raw.DXBridge_EndFrame(device), 'DXBridge_EndFrame');
  }

  present(swapChain, syncInterval) {
    this.requireOK(this.raw.DXBridge_Present(swapChain, syncInterval), 'DXBridge_Present');
  }
}

module.exports = {
  DXBRIDGE_NULL_HANDLE,
  DXBRIDGE_STRUCT_VERSION,
  DXB_OK,
  DXB_E_INVALID_HANDLE,
  DXB_E_DEVICE_LOST,
  DXB_E_OUT_OF_MEMORY,
  DXB_E_INVALID_ARG,
  DXB_E_NOT_SUPPORTED,
  DXB_E_SHADER_COMPILE,
  DXB_E_NOT_INITIALIZED,
  DXB_E_INVALID_STATE,
  DXB_E_INTERNAL,
  DXB_BACKEND_AUTO,
  DXB_BACKEND_DX11,
  DXB_BACKEND_DX12,
  DXB_FORMAT_UNKNOWN,
  DXB_FORMAT_RGBA8_UNORM,
  DXB_FORMAT_RGBA16_FLOAT,
  DXB_FORMAT_D32_FLOAT,
  DXB_FORMAT_D24_UNORM_S8,
  DXB_FORMAT_R32_UINT,
  DXB_FORMAT_RG32_FLOAT,
  DXB_FORMAT_RGB32_FLOAT,
  DXB_FORMAT_RGBA32_FLOAT,
  DXB_DEVICE_FLAG_DEBUG,
  DXB_DEVICE_FLAG_GPU_VALIDATION,
  DXB_COMPILE_DEBUG,
  DXB_BUFFER_FLAG_VERTEX,
  DXB_BUFFER_FLAG_INDEX,
  DXB_BUFFER_FLAG_CONSTANT,
  DXB_BUFFER_FLAG_DYNAMIC,
  DXB_SHADER_STAGE_VERTEX,
  DXB_SHADER_STAGE_PIXEL,
  DXB_PRIM_TRIANGLELIST,
  DXB_PRIM_TRIANGLESTRIP,
  DXB_PRIM_LINELIST,
  DXB_PRIM_POINTLIST,
  DXB_SEMANTIC_POSITION,
  DXB_SEMANTIC_NORMAL,
  DXB_SEMANTIC_TEXCOORD,
  DXB_SEMANTIC_COLOR,
  DXB_FEATURE_DX12,
  DXB_RESULT_NAMES,
  DXB_LOG_LEVEL_NAMES,
  DXB_BACKEND_NAMES,
  DXBridgeError,
  DXBridgeLibrary,
  formatBytes,
  formatHRESULT,
  getBooleanArg,
  getStringArg,
  locateDXBridgeDLL,
  parseArgs,
  parseBackend,
  parseVersion
};
