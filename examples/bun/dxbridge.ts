import { existsSync } from "node:fs";
import path from "node:path";
import { CString, JSCallback, dlopen, ptr } from "bun:ffi";

export const DXBRIDGE_NULL_HANDLE = 0n;
export const DXBRIDGE_STRUCT_VERSION = 1;

export const DXB_OK = 0;
export const DXB_E_INVALID_HANDLE = -1;
export const DXB_E_DEVICE_LOST = -2;
export const DXB_E_OUT_OF_MEMORY = -3;
export const DXB_E_INVALID_ARG = -4;
export const DXB_E_NOT_SUPPORTED = -5;
export const DXB_E_SHADER_COMPILE = -6;
export const DXB_E_NOT_INITIALIZED = -7;
export const DXB_E_INVALID_STATE = -8;
export const DXB_E_INTERNAL = -999;

export const DXB_BACKEND_AUTO = 0;
export const DXB_BACKEND_DX11 = 1;
export const DXB_BACKEND_DX12 = 2;

export const DXB_FORMAT_UNKNOWN = 0;
export const DXB_FORMAT_RGBA8_UNORM = 1;
export const DXB_FORMAT_RGBA16_FLOAT = 2;
export const DXB_FORMAT_D32_FLOAT = 3;
export const DXB_FORMAT_D24_UNORM_S8 = 4;
export const DXB_FORMAT_R32_UINT = 5;
export const DXB_FORMAT_RG32_FLOAT = 6;
export const DXB_FORMAT_RGB32_FLOAT = 7;
export const DXB_FORMAT_RGBA32_FLOAT = 8;

export const DXB_DEVICE_FLAG_DEBUG = 0x0001;
export const DXB_DEVICE_FLAG_GPU_VALIDATION = 0x0002;

export const DXB_COMPILE_DEBUG = 0x0001;

export const DXB_BUFFER_FLAG_VERTEX = 0x0001;
export const DXB_BUFFER_FLAG_INDEX = 0x0002;
export const DXB_BUFFER_FLAG_CONSTANT = 0x0004;
export const DXB_BUFFER_FLAG_DYNAMIC = 0x0008;

export const DXB_SHADER_STAGE_VERTEX = 0;
export const DXB_SHADER_STAGE_PIXEL = 1;

export const DXB_PRIM_TRIANGLELIST = 0;
export const DXB_PRIM_TRIANGLESTRIP = 1;
export const DXB_PRIM_LINELIST = 2;
export const DXB_PRIM_POINTLIST = 3;

export const DXB_SEMANTIC_POSITION = 0;
export const DXB_SEMANTIC_NORMAL = 1;
export const DXB_SEMANTIC_TEXCOORD = 2;
export const DXB_SEMANTIC_COLOR = 3;

export const DXB_FEATURE_DX12 = 0x0001;

export const DXB_CAPABILITY_BACKEND_AVAILABLE = 1;
export const DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE = 2;
export const DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE = 3;
export const DXB_CAPABILITY_ADAPTER_COUNT = 4;
export const DXB_CAPABILITY_ADAPTER_SOFTWARE = 5;
export const DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL = 6;

export const DXB_RESULT_NAMES: Record<number, string> = {
  [DXB_OK]: "DXB_OK",
  [DXB_E_INVALID_HANDLE]: "DXB_E_INVALID_HANDLE",
  [DXB_E_DEVICE_LOST]: "DXB_E_DEVICE_LOST",
  [DXB_E_OUT_OF_MEMORY]: "DXB_E_OUT_OF_MEMORY",
  [DXB_E_INVALID_ARG]: "DXB_E_INVALID_ARG",
  [DXB_E_NOT_SUPPORTED]: "DXB_E_NOT_SUPPORTED",
  [DXB_E_SHADER_COMPILE]: "DXB_E_SHADER_COMPILE",
  [DXB_E_NOT_INITIALIZED]: "DXB_E_NOT_INITIALIZED",
  [DXB_E_INVALID_STATE]: "DXB_E_INVALID_STATE",
  [DXB_E_INTERNAL]: "DXB_E_INTERNAL",
};

export const DXB_LOG_LEVEL_NAMES: Record<number, string> = {
  0: "INFO",
  1: "WARN",
  2: "ERROR",
};

export const DXB_BACKEND_NAMES: Record<string, number> = {
  auto: DXB_BACKEND_AUTO,
  dx11: DXB_BACKEND_DX11,
  dx12: DXB_BACKEND_DX12,
};

const DXB_ADAPTER_INFO_SIZE = 168;
const DXB_DEVICE_DESC_SIZE = 16;
const DXB_SWAPCHAIN_DESC_SIZE = 40;
const DXB_BUFFER_DESC_SIZE = 16;
const DXB_SHADER_DESC_SIZE = 56;
const DXB_INPUT_ELEMENT_DESC_SIZE = 24;
const DXB_PIPELINE_DESC_SIZE = 56;
const DXB_VIEWPORT_SIZE = 24;
const DXB_CAPABILITY_QUERY_SIZE = 48;
const DXB_CAPABILITY_INFO_SIZE = 48;

export type DXBHandle = bigint;

export interface AdapterInfo {
  structVersion: number;
  index: number;
  description: string;
  dedicatedVideoMem: bigint;
  dedicatedSystemMem: bigint;
  sharedSystemMem: bigint;
  isSoftware: boolean;
}

export interface DeviceDescInit {
  backend: number;
  adapterIndex?: number;
  flags?: number;
}

export interface SwapChainDescInit {
  hwnd: number | bigint;
  width: number;
  height: number;
  format?: number;
  bufferCount?: number;
  flags?: number;
}

export interface BufferDescInit {
  flags: number;
  byteSize: number;
  stride?: number;
}

export interface ShaderDescInit {
  stage: number;
  sourceCode: string;
  sourceName?: string;
  entryPoint?: string;
  target: string;
  compileFlags?: number;
}

export interface InputElementDescInit {
  semantic: number;
  semanticIndex?: number;
  format: number;
  inputSlot?: number;
  byteOffset: number;
}

export interface PipelineDescInit {
  vs: DXBHandle;
  ps: DXBHandle;
  inputLayout: DXBHandle;
  topology?: number;
  depthTest?: number;
  depthWrite?: number;
  cullBack?: number;
  wireframe?: number;
}

export interface ViewportInit {
  x?: number;
  y?: number;
  width: number;
  height: number;
  minDepth?: number;
  maxDepth?: number;
}

export interface CapabilityQueryInit {
  capability: number;
  backend: number;
  adapterIndex?: number;
  format?: number;
  device?: DXBHandle;
}

export interface CapabilityInfo {
  structVersion: number;
  capability: number;
  backend: number;
  adapterIndex: number;
  supported: boolean;
  valueU32: number;
  valueU64: bigint;
}

export interface ParsedArgs {
  positional: string[];
  values: Record<string, string | boolean>;
}

export class DXBridgeError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "DXBridgeError";
  }
}

const DXBRIDGE_SYMBOLS = {
  DXBridge_SetLogCallback: { args: ["ptr", "ptr"], returns: "void" },
  DXBridge_Init: { args: ["u32"], returns: "i32" },
  DXBridge_Shutdown: { args: [], returns: "void" },
  DXBridge_GetVersion: { args: [], returns: "u32" },
  DXBridge_SupportsFeature: { args: ["u32"], returns: "u32" },
  DXBridge_QueryCapability: { args: ["ptr", "ptr"], returns: "i32" },
  DXBridge_GetLastError: { args: ["ptr", "i32"], returns: "void" },
  DXBridge_GetLastHRESULT: { args: [], returns: "u32" },
  DXBridge_EnumerateAdapters: { args: ["ptr", "ptr"], returns: "i32" },
  DXBridge_CreateDevice: { args: ["ptr", "ptr"], returns: "i32" },
  DXBridge_DestroyDevice: { args: ["u64"], returns: "void" },
  DXBridge_GetFeatureLevel: { args: ["u64"], returns: "u32" },
  DXBridge_CreateSwapChain: { args: ["u64", "ptr", "ptr"], returns: "i32" },
  DXBridge_DestroySwapChain: { args: ["u64"], returns: "void" },
  DXBridge_ResizeSwapChain: { args: ["u64", "u32", "u32"], returns: "i32" },
  DXBridge_GetBackBuffer: { args: ["u64", "ptr"], returns: "i32" },
  DXBridge_CreateRenderTargetView: { args: ["u64", "u64", "ptr"], returns: "i32" },
  DXBridge_DestroyRTV: { args: ["u64"], returns: "void" },
  DXBridge_CreateDepthStencilView: { args: ["u64", "ptr", "ptr"], returns: "i32" },
  DXBridge_DestroyDSV: { args: ["u64"], returns: "void" },
  DXBridge_CompileShader: { args: ["u64", "ptr", "ptr"], returns: "i32" },
  DXBridge_DestroyShader: { args: ["u64"], returns: "void" },
  DXBridge_CreateInputLayout: { args: ["u64", "ptr", "u32", "u64", "ptr"], returns: "i32" },
  DXBridge_DestroyInputLayout: { args: ["u64"], returns: "void" },
  DXBridge_CreatePipelineState: { args: ["u64", "ptr", "ptr"], returns: "i32" },
  DXBridge_DestroyPipeline: { args: ["u64"], returns: "void" },
  DXBridge_CreateBuffer: { args: ["u64", "ptr", "ptr"], returns: "i32" },
  DXBridge_UploadData: { args: ["u64", "ptr", "u32"], returns: "i32" },
  DXBridge_DestroyBuffer: { args: ["u64"], returns: "void" },
  DXBridge_BeginFrame: { args: ["u64"], returns: "i32" },
  DXBridge_SetRenderTargets: { args: ["u64", "u64", "u64"], returns: "i32" },
  DXBridge_ClearRenderTarget: { args: ["u64", "ptr"], returns: "i32" },
  DXBridge_ClearDepthStencil: { args: ["u64", "f32", "u8"], returns: "i32" },
  DXBridge_SetViewport: { args: ["u64", "ptr"], returns: "i32" },
  DXBridge_SetPipeline: { args: ["u64", "u64"], returns: "i32" },
  DXBridge_SetVertexBuffer: { args: ["u64", "u64", "u32", "u32"], returns: "i32" },
  DXBridge_SetIndexBuffer: { args: ["u64", "u64", "u32", "u32"], returns: "i32" },
  DXBridge_SetConstantBuffer: { args: ["u64", "u32", "u64"], returns: "i32" },
  DXBridge_Draw: { args: ["u64", "u32", "u32"], returns: "i32" },
  DXBridge_DrawIndexed: { args: ["u64", "u32", "u32", "i32"], returns: "i32" },
  DXBridge_EndFrame: { args: ["u64"], returns: "i32" },
  DXBridge_Present: { args: ["u64", "u32"], returns: "i32" },
  DXBridge_GetNativeDevice: { args: ["u64"], returns: "ptr" },
  DXBridge_GetNativeContext: { args: ["u64"], returns: "ptr" },
} as const;

function assertWindows(): void {
  if (process.platform !== "win32") {
    throw new DXBridgeError("These Bun examples require Windows because dxbridge.dll is a Windows DLL.");
  }
}

function decodeCString(bytes: Uint8Array): string {
  const end = bytes.indexOf(0);
  const slice = end >= 0 ? bytes.subarray(0, end) : bytes;
  return new TextDecoder().decode(slice);
}

function writeU32(view: DataView, offset: number, value: number): void {
  view.setUint32(offset, value >>> 0, true);
}

function writeU64(view: DataView, offset: number, value: bigint): void {
  view.setBigUint64(offset, value, true);
}

function writePtr(view: DataView, offset: number, value: number | bigint): void {
  writeU64(view, offset, typeof value === "bigint" ? value : BigInt(value));
}

function writeF32(view: DataView, offset: number, value: number): void {
  view.setFloat32(offset, value, true);
}

function readU64(view: DataView, offset: number): bigint {
  return view.getBigUint64(offset, true);
}

function encodeDeviceDesc(desc: DeviceDescInit): Uint8Array {
  const bytes = new Uint8Array(DXB_DEVICE_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(view, 4, desc.backend);
  writeU32(view, 8, desc.adapterIndex ?? 0);
  writeU32(view, 12, desc.flags ?? 0);
  return bytes;
}

function encodeSwapChainDesc(desc: SwapChainDescInit): Uint8Array {
  const bytes = new Uint8Array(DXB_SWAPCHAIN_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writePtr(view, 8, desc.hwnd);
  writeU32(view, 16, desc.width);
  writeU32(view, 20, desc.height);
  writeU32(view, 24, desc.format ?? DXB_FORMAT_RGBA8_UNORM);
  writeU32(view, 28, desc.bufferCount ?? 2);
  writeU32(view, 32, desc.flags ?? 0);
  return bytes;
}

function encodeBufferDesc(desc: BufferDescInit): Uint8Array {
  const bytes = new Uint8Array(DXB_BUFFER_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(view, 4, desc.flags);
  writeU32(view, 8, desc.byteSize);
  writeU32(view, 12, desc.stride ?? 0);
  return bytes;
}

function createCStringBuffer(text: string): Uint8Array {
  return new Uint8Array(Buffer.from(`${text}\0`, "utf8"));
}

function encodeShaderDesc(desc: ShaderDescInit) {
  const bytes = new Uint8Array(DXB_SHADER_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const sourceCode = createCStringBuffer(desc.sourceCode);
  const sourceName = createCStringBuffer(desc.sourceName ?? "shader");
  const entryPoint = createCStringBuffer(desc.entryPoint ?? "main");
  const target = createCStringBuffer(desc.target);

  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(view, 4, desc.stage);
  writePtr(view, 8, ptr(sourceCode));
  writeU32(view, 16, 0);
  writePtr(view, 24, ptr(sourceName));
  writePtr(view, 32, ptr(entryPoint));
  writePtr(view, 40, ptr(target));
  writeU32(view, 48, desc.compileFlags ?? 0);

  return { bytes, sourceCode, sourceName, entryPoint, target };
}

function encodeInputElements(elements: InputElementDescInit[]): Uint8Array {
  const bytes = new Uint8Array(elements.length * DXB_INPUT_ELEMENT_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let index = 0; index < elements.length; index += 1) {
    const base = index * DXB_INPUT_ELEMENT_DESC_SIZE;
    const element = elements[index]!;
    writeU32(view, base + 0, DXBRIDGE_STRUCT_VERSION);
    writeU32(view, base + 4, element.semantic);
    writeU32(view, base + 8, element.semanticIndex ?? 0);
    writeU32(view, base + 12, element.format);
    writeU32(view, base + 16, element.inputSlot ?? 0);
    writeU32(view, base + 20, element.byteOffset);
  }
  return bytes;
}

function encodePipelineDesc(desc: PipelineDescInit): Uint8Array {
  const bytes = new Uint8Array(DXB_PIPELINE_DESC_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writeU64(view, 8, desc.vs);
  writeU64(view, 16, desc.ps);
  writeU64(view, 24, desc.inputLayout);
  writeU32(view, 32, desc.topology ?? DXB_PRIM_TRIANGLELIST);
  writeU32(view, 36, desc.depthTest ?? 0);
  writeU32(view, 40, desc.depthWrite ?? 0);
  writeU32(view, 44, desc.cullBack ?? 0);
  writeU32(view, 48, desc.wireframe ?? 0);
  return bytes;
}

function encodeViewport(viewport: ViewportInit): Uint8Array {
  const bytes = new Uint8Array(DXB_VIEWPORT_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeF32(view, 0, viewport.x ?? 0);
  writeF32(view, 4, viewport.y ?? 0);
  writeF32(view, 8, viewport.width);
  writeF32(view, 12, viewport.height);
  writeF32(view, 16, viewport.minDepth ?? 0);
  writeF32(view, 20, viewport.maxDepth ?? 1);
  return bytes;
}

function encodeCapabilityQuery(query: CapabilityQueryInit): Uint8Array {
  const bytes = new Uint8Array(DXB_CAPABILITY_QUERY_SIZE);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  writeU32(view, 0, DXBRIDGE_STRUCT_VERSION);
  writeU32(view, 4, query.capability);
  writeU32(view, 8, query.backend);
  writeU32(view, 12, query.adapterIndex ?? 0);
  writeU32(view, 16, query.format ?? DXB_FORMAT_UNKNOWN);
  // offset 20: 4 bytes of implicit padding to align the 64-bit `device` field to offset 24.
  // `new Uint8Array` zero-fills, so the padding is correctly zeroed without an explicit write.
  writeU64(view, 24, query.device ?? DXBRIDGE_NULL_HANDLE);
  return bytes;
}

function decodeCapabilityInfo(bytes: Uint8Array): CapabilityInfo {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return {
    structVersion: view.getUint32(0, true),
    capability: view.getUint32(4, true),
    backend: view.getUint32(8, true),
    adapterIndex: view.getUint32(12, true),
    supported: view.getUint32(16, true) !== 0,
    valueU32: view.getUint32(20, true),
    valueU64: readU64(view, 24),
  };
}

function toByteView(data: ArrayBufferView): Uint8Array {
  return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
}

function formatSearched(candidates: string[]): string {
  return candidates.map((candidate) => `  - ${candidate}`).join("\n");
}

export function parseVersion(version: number): string {
  const major = (version >>> 16) & 0xff;
  const minor = (version >>> 8) & 0xff;
  const patch = version & 0xff;
  return `${major}.${minor}.${patch}`;
}

export function formatHRESULT(value: number): string {
  return `0x${(value >>> 0).toString(16).toUpperCase().padStart(8, "0")}`;
}

export function formatBytes(value: bigint): string {
  const units = ["B", "KiB", "MiB", "GiB", "TiB"];
  let amount = Number(value);
  let unit = units[0];
  for (const candidate of units) {
    unit = candidate;
    if (amount < 1024 || candidate === units[units.length - 1]) {
      break;
    }
    amount /= 1024;
  }
  if (unit === "B") {
    return `${Math.trunc(amount)} ${unit}`;
  }
  return `${amount.toFixed(1)} ${unit}`;
}

export function formatFeatureLevel(value: number): string {
  if (value === 0) {
    return "n/a";
  }
  const major = (value >>> 12) & 0xf;
  const minor = (value >>> 8) & 0xf;
  if (major === 0 && minor === 0) {
    return formatHRESULT(value);
  }
  return `${major}_${minor}`;
}

export function parseBackend(name: string): number {
  const backend = DXB_BACKEND_NAMES[name.toLowerCase()];
  if (backend === undefined) {
    throw new DXBridgeError(`Unknown backend ${JSON.stringify(name)}. Use auto, dx11, or dx12.`);
  }
  return backend;
}

export function parseArgs(argv: string[]): ParsedArgs {
  const positional: string[] = [];
  const values: Record<string, string | boolean> = {};

  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];
    if (!token.startsWith("--")) {
      positional.push(token);
      continue;
    }

    const body = token.slice(2);
    const equalsIndex = body.indexOf("=");
    if (equalsIndex >= 0) {
      values[body.slice(0, equalsIndex)] = body.slice(equalsIndex + 1);
      continue;
    }

    const next = argv[index + 1];
    if (next && !next.startsWith("--")) {
      values[body] = next;
      index += 1;
      continue;
    }

    values[body] = true;
  }

  return { positional, values };
}

export function getStringArg(args: ParsedArgs, key: string, fallback = ""): string {
  const value = args.values[key];
  return typeof value === "string" ? value : fallback;
}

export function getBooleanArg(args: ParsedArgs, key: string): boolean {
  return args.values[key] === true;
}

export function locateDXBridgeDLL(explicitPath?: string): string {
  assertWindows();

  const here = import.meta.dir;
  const repoRoot = path.resolve(here, "..", "..");
  const candidates = [
    explicitPath,
    process.env.DXBRIDGE_DLL,
    path.join(process.cwd(), "dxbridge.dll"),
    path.join(here, "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "debug", "Debug", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "debug", "examples", "Debug", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "debug", "tests", "Debug", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "release", "Release", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "release", "examples", "Release", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "release", "tests", "Release", "dxbridge.dll"),
    path.join(repoRoot, "out", "build", "ci", "Release", "dxbridge.dll"),
  ].filter((candidate): candidate is string => Boolean(candidate));

  for (const candidate of candidates) {
    const resolved = path.resolve(candidate);
    if (existsSync(resolved)) {
      return resolved;
    }
  }

  throw new DXBridgeError(
    `Could not locate dxbridge.dll. Set DXBRIDGE_DLL or pass --dll.\nSearched:\n${formatSearched(candidates)}`,
  );
}

export class DXBridgeLibrary {
  readonly path: string;
  readonly raw: any;

  #closeLibrary: () => void;
  #logCallback: JSCallback | null = null;
  #closed = false;

  constructor(explicitPath?: string) {
    this.path = locateDXBridgeDLL(explicitPath);
    const library = dlopen(this.path, DXBRIDGE_SYMBOLS);
    this.raw = library.symbols;
    this.#closeLibrary = library.close;
  }

  close(): void {
    if (this.#closed) {
      return;
    }
    this.setLogCallback(null);
    this.#closeLibrary();
    this.#closed = true;
  }

  setLogCallback(handler: ((level: number, message: string, userData: number) => void) | null): void {
    if (handler === null) {
      this.raw.DXBridge_SetLogCallback(0, 0);
      this.#logCallback?.close();
      this.#logCallback = null;
      return;
    }

    this.#logCallback?.close();
    this.#logCallback = new JSCallback(
      (level: number, messagePtr: number, userData: number) => {
        const message = messagePtr === 0 ? "" : new CString(messagePtr).toString();
        handler(level, message, userData);
      },
      { returns: "void", args: ["i32", "ptr", "ptr"] },
    );
    this.raw.DXBridge_SetLogCallback(this.#logCallback.ptr, 0);
  }

  getVersion(): number {
    return this.raw.DXBridge_GetVersion();
  }

  supportsFeature(featureFlag: number): boolean {
    return this.raw.DXBridge_SupportsFeature(featureFlag) !== 0;
  }

  queryCapability(query: CapabilityQueryInit): CapabilityInfo {
    const queryBuffer = encodeCapabilityQuery(query);
    const infoBuffer = new Uint8Array(DXB_CAPABILITY_INFO_SIZE);
    const infoView = new DataView(infoBuffer.buffer, infoBuffer.byteOffset, infoBuffer.byteLength);
    writeU32(infoView, 0, DXBRIDGE_STRUCT_VERSION);
    this.requireOK(this.raw.DXBridge_QueryCapability(queryBuffer, infoBuffer), "DXBridge_QueryCapability");
    return decodeCapabilityInfo(infoBuffer);
  }

  queryBackendAvailable(backend: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_BACKEND_AVAILABLE, backend });
  }

  queryDebugLayerAvailable(backend: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE, backend });
  }

  queryGPUValidationAvailable(backend: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE, backend });
  }

  queryAdapterCount(backend: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_ADAPTER_COUNT, backend });
  }

  queryAdapterSoftware(backend: number, adapterIndex: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_ADAPTER_SOFTWARE, backend, adapterIndex });
  }

  queryAdapterMaxFeatureLevel(backend: number, adapterIndex: number): CapabilityInfo {
    return this.queryCapability({ capability: DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL, backend, adapterIndex });
  }

  init(backendHint: number): void {
    this.requireOK(this.raw.DXBridge_Init(backendHint), "DXBridge_Init");
  }

  shutdown(): void {
    this.raw.DXBridge_Shutdown();
  }

  getLastError(): string {
    const buffer = new Uint8Array(512);
    this.raw.DXBridge_GetLastError(buffer, buffer.byteLength);
    return decodeCString(buffer);
  }

  getLastHRESULT(): number {
    return this.raw.DXBridge_GetLastHRESULT();
  }

  describeFailure(result: number): string {
    const name = DXB_RESULT_NAMES[result] ?? `DXBResult(${result})`;
    const message = this.getLastError();
    const hresult = formatHRESULT(this.getLastHRESULT());
    return message ? `${name}: ${message} (HRESULT ${hresult})` : `${name} (HRESULT ${hresult})`;
  }

  requireOK(result: number, functionName: string): void {
    if (result === DXB_OK) {
      return;
    }
    throw new DXBridgeError(`${functionName} failed: ${this.describeFailure(result)}`);
  }

  enumerateAdapters(): AdapterInfo[] {
    const countRef = new Uint32Array(1);
    this.requireOK(this.raw.DXBridge_EnumerateAdapters(0, countRef), "DXBridge_EnumerateAdapters(count)");

    const count = countRef[0] ?? 0;
    if (count === 0) {
      return [];
    }

    const buffer = new Uint8Array(count * DXB_ADAPTER_INFO_SIZE);
    const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
    for (let index = 0; index < count; index += 1) {
      writeU32(view, index * DXB_ADAPTER_INFO_SIZE, DXBRIDGE_STRUCT_VERSION);
    }

    countRef[0] = count;
    this.requireOK(this.raw.DXBridge_EnumerateAdapters(buffer, countRef), "DXBridge_EnumerateAdapters(fill)");

    const finalCount = countRef[0] ?? count;
    const adapters: AdapterInfo[] = [];
    for (let index = 0; index < finalCount; index += 1) {
      const base = index * DXB_ADAPTER_INFO_SIZE;
      const description = decodeCString(buffer.subarray(base + 8, base + 136));
      adapters.push({
        structVersion: view.getUint32(base, true),
        index: view.getUint32(base + 4, true),
        description,
        dedicatedVideoMem: readU64(view, base + 136),
        dedicatedSystemMem: readU64(view, base + 144),
        sharedSystemMem: readU64(view, base + 152),
        isSoftware: view.getUint32(base + 160, true) !== 0,
      });
    }
    return adapters;
  }

  createDeviceCall(descBuffer: Uint8Array | null, outDevice = new BigUint64Array(1)): number {
    return this.raw.DXBridge_CreateDevice(descBuffer ?? 0, outDevice);
  }

  createDevice(desc: DeviceDescInit): DXBHandle {
    const descBuffer = encodeDeviceDesc(desc);
    const outDevice = new BigUint64Array(1);
    this.requireOK(this.createDeviceCall(descBuffer, outDevice), "DXBridge_CreateDevice");
    return outDevice[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyDevice(device: DXBHandle): void {
    this.raw.DXBridge_DestroyDevice(device);
  }

  getFeatureLevel(device: DXBHandle): number {
    return this.raw.DXBridge_GetFeatureLevel(device);
  }

  createSwapChain(device: DXBHandle, desc: SwapChainDescInit): DXBHandle {
    const descBuffer = encodeSwapChainDesc(desc);
    const outSwapChain = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_CreateSwapChain(device, descBuffer, outSwapChain), "DXBridge_CreateSwapChain");
    return outSwapChain[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroySwapChain(swapChain: DXBHandle): void {
    this.raw.DXBridge_DestroySwapChain(swapChain);
  }

  getBackBuffer(swapChain: DXBHandle): DXBHandle {
    const outRenderTarget = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_GetBackBuffer(swapChain, outRenderTarget), "DXBridge_GetBackBuffer");
    return outRenderTarget[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  createRenderTargetView(device: DXBHandle, renderTarget: DXBHandle): DXBHandle {
    const outRTV = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_CreateRenderTargetView(device, renderTarget, outRTV), "DXBridge_CreateRenderTargetView");
    return outRTV[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyRenderTargetView(rtv: DXBHandle): void {
    this.raw.DXBridge_DestroyRTV(rtv);
  }

  compileShader(device: DXBHandle, desc: ShaderDescInit): DXBHandle {
    const encoded = encodeShaderDesc(desc);
    const outShader = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_CompileShader(device, encoded.bytes, outShader), "DXBridge_CompileShader");
    return outShader[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyShader(shader: DXBHandle): void {
    this.raw.DXBridge_DestroyShader(shader);
  }

  createInputLayout(device: DXBHandle, elements: InputElementDescInit[], vertexShader: DXBHandle): DXBHandle {
    if (elements.length === 0) {
      throw new DXBridgeError("createInputLayout requires at least one element.");
    }
    const buffer = encodeInputElements(elements);
    const outLayout = new BigUint64Array(1);
    this.requireOK(
      this.raw.DXBridge_CreateInputLayout(device, buffer, elements.length, vertexShader, outLayout),
      "DXBridge_CreateInputLayout",
    );
    return outLayout[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyInputLayout(layout: DXBHandle): void {
    this.raw.DXBridge_DestroyInputLayout(layout);
  }

  createPipelineState(device: DXBHandle, desc: PipelineDescInit): DXBHandle {
    const buffer = encodePipelineDesc(desc);
    const outPipeline = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_CreatePipelineState(device, buffer, outPipeline), "DXBridge_CreatePipelineState");
    return outPipeline[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  destroyPipeline(pipeline: DXBHandle): void {
    this.raw.DXBridge_DestroyPipeline(pipeline);
  }

  createBuffer(device: DXBHandle, desc: BufferDescInit): DXBHandle {
    const bufferDesc = encodeBufferDesc(desc);
    const outBuffer = new BigUint64Array(1);
    this.requireOK(this.raw.DXBridge_CreateBuffer(device, bufferDesc, outBuffer), "DXBridge_CreateBuffer");
    return outBuffer[0] ?? DXBRIDGE_NULL_HANDLE;
  }

  uploadData(buffer: DXBHandle, data: ArrayBufferView): void {
    const bytes = toByteView(data);
    if (bytes.byteLength === 0) {
      throw new DXBridgeError("uploadData requires a non-empty typed array.");
    }
    this.requireOK(this.raw.DXBridge_UploadData(buffer, bytes, bytes.byteLength), "DXBridge_UploadData");
  }

  destroyBuffer(buffer: DXBHandle): void {
    this.raw.DXBridge_DestroyBuffer(buffer);
  }

  beginFrame(device: DXBHandle): void {
    this.requireOK(this.raw.DXBridge_BeginFrame(device), "DXBridge_BeginFrame");
  }

  setRenderTargets(device: DXBHandle, rtv: DXBHandle, dsv = DXBRIDGE_NULL_HANDLE): void {
    this.requireOK(this.raw.DXBridge_SetRenderTargets(device, rtv, dsv), "DXBridge_SetRenderTargets");
  }

  clearRenderTarget(rtv: DXBHandle, rgba: readonly [number, number, number, number]): void {
    const color = new Float32Array(rgba);
    this.requireOK(this.raw.DXBridge_ClearRenderTarget(rtv, color), "DXBridge_ClearRenderTarget");
  }

  setViewport(device: DXBHandle, viewport: ViewportInit): void {
    const bytes = encodeViewport(viewport);
    this.requireOK(this.raw.DXBridge_SetViewport(device, bytes), "DXBridge_SetViewport");
  }

  setPipeline(device: DXBHandle, pipeline: DXBHandle): void {
    this.requireOK(this.raw.DXBridge_SetPipeline(device, pipeline), "DXBridge_SetPipeline");
  }

  setVertexBuffer(device: DXBHandle, buffer: DXBHandle, stride: number, offset = 0): void {
    this.requireOK(this.raw.DXBridge_SetVertexBuffer(device, buffer, stride, offset), "DXBridge_SetVertexBuffer");
  }

  draw(device: DXBHandle, vertexCount: number, startVertex = 0): void {
    this.requireOK(this.raw.DXBridge_Draw(device, vertexCount, startVertex), "DXBridge_Draw");
  }

  endFrame(device: DXBHandle): void {
    this.requireOK(this.raw.DXBridge_EndFrame(device), "DXBridge_EndFrame");
  }

  present(swapChain: DXBHandle, syncInterval: number): void {
    this.requireOK(this.raw.DXBridge_Present(swapChain, syncInterval), "DXBridge_Present");
  }
}
