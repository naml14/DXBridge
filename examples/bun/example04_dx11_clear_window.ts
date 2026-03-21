import {
  DXB_BACKEND_DX11,
  DXB_DEVICE_FLAG_DEBUG,
  DXBRIDGE_NULL_HANDLE,
  DXBridgeLibrary,
  getBooleanArg,
  getStringArg,
  parseArgs,
} from "./dxbridge";
import { Win32Window } from "./win32";

function printUsage(): void {
  console.log(
    "Usage: bun run examples/bun/example04_dx11_clear_window.ts [--dll PATH] [--width 640] [--height 360] [--frames 180] [--color R,G,B,A] [--hidden] [--debug]",
  );
}

function parseRGBA(value: string): [number, number, number, number] {
  const parts = value.split(",").map((part) => Number.parseFloat(part.trim()));
  if (parts.length !== 4 || parts.some((part) => Number.isNaN(part))) {
    throw new Error("--color must be R,G,B,A");
  }
  return [parts[0]!, parts[1]!, parts[2]!, parts[3]!];
}

function main(): void {
  const args = parseArgs(Bun.argv.slice(2));
  if (args.values.help === true) {
    printUsage();
    return;
  }

  const dllPath = getStringArg(args, "dll");
  const width = Number.parseInt(getStringArg(args, "width", "640"), 10);
  const height = Number.parseInt(getStringArg(args, "height", "360"), 10);
  const frames = Number.parseInt(getStringArg(args, "frames", "180"), 10);
  const clearColor = parseRGBA(getStringArg(args, "color", "0.08,0.16,0.32,1.0"));
  const hidden = getBooleanArg(args, "hidden");
  const debug = getBooleanArg(args, "debug");

  const window = new Win32Window("DXBridge Bun Clear Window", width, height, !hidden, "DXBridgeBunClearWindow");
  const dxbridge = new DXBridgeLibrary(dllPath || undefined);

  let device = DXBRIDGE_NULL_HANDLE;
  let swapChain = DXBRIDGE_NULL_HANDLE;
  let rtv = DXBRIDGE_NULL_HANDLE;

  try {
    const hwnd = window.create();
    console.log(`Loaded: ${dxbridge.path}`);
    console.log(`Created HWND: 0x${hwnd.toString(16).toUpperCase().padStart(16, "0")}`);

    dxbridge.init(DXB_BACKEND_DX11);
    device = dxbridge.createDevice({
      backend: DXB_BACKEND_DX11,
      adapterIndex: 0,
      flags: debug ? DXB_DEVICE_FLAG_DEBUG : 0,
    });
    swapChain = dxbridge.createSwapChain(device, {
      hwnd,
      width,
      height,
    });

    const backBuffer = dxbridge.getBackBuffer(swapChain);
    rtv = dxbridge.createRenderTargetView(device, backBuffer);

    console.log("Rendering frames. Close the window manually or wait for auto-close.");
    for (let frame = 0; frame < frames; frame += 1) {
      if (!window.pumpMessages()) {
        break;
      }
      dxbridge.beginFrame(device);
      dxbridge.setRenderTargets(device, rtv);
      dxbridge.clearRenderTarget(rtv, clearColor);
      dxbridge.endFrame(device);
      dxbridge.present(swapChain, 1);
      if (frame + 1 === frames) {
        window.close();
      }
    }

    console.log("Done.");
  } finally {
    if (rtv !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyRenderTargetView(rtv);
    }
    if (swapChain !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroySwapChain(swapChain);
    }
    if (device !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyDevice(device);
    }
    dxbridge.shutdown();
    dxbridge.close();
    window.dispose();
  }
}

main();
