const {
  DXB_BACKEND_DX11,
  DXB_BUFFER_FLAG_DYNAMIC,
  DXB_BUFFER_FLAG_VERTEX,
  DXB_COMPILE_DEBUG,
  DXB_DEVICE_FLAG_DEBUG,
  DXB_FORMAT_RG32_FLOAT,
  DXB_FORMAT_RGB32_FLOAT,
  DXB_PRIM_TRIANGLELIST,
  DXB_SEMANTIC_COLOR,
  DXB_SEMANTIC_POSITION,
  DXB_SHADER_STAGE_PIXEL,
  DXB_SHADER_STAGE_VERTEX,
  DXBRIDGE_NULL_HANDLE,
  DXBridgeLibrary,
  getBooleanArg,
  getStringArg,
  parseArgs
} = require('./dxbridge');
const { Win32Window } = require('./win32');

const vertexShaderSource = `
struct VSInput {
    float2 pos : POSITION;
    float3 col : COLOR;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

PSInput main(VSInput v) {
    PSInput o;
    o.pos = float4(v.pos, 0.0f, 1.0f);
    o.col = v.col;
    return o;
}
`;

const pixelShaderSource = `
struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

float4 main(PSInput p) : SV_TARGET {
    return float4(p.col, 1.0f);
}
`;

function printUsage() {
  console.log(
    'Usage: node examples/nodejs/example05_dx11_triangle.js [--dll PATH] [--width 960] [--height 540] [--frames 0] [--speed 1.6] [--amplitude 0.38] [--hidden] [--debug] [--sync-interval 1]'
  );
}

function makeVertexData(offsetX, offsetY) {
  return new Float32Array([
    0.0 + offsetX, 0.52 + offsetY, 1.0, 0.1, 0.1,
    -0.42 + offsetX, -0.38 + offsetY, 0.1, 1.0, 0.1,
    0.42 + offsetX, -0.38 + offsetY, 0.1, 0.3, 1.0
  ]);
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.values.help === true) {
    printUsage();
    return;
  }

  const dllPath = getStringArg(args, 'dll');
  const width = Number.parseInt(getStringArg(args, 'width', '960'), 10);
  const height = Number.parseInt(getStringArg(args, 'height', '540'), 10);
  const frames = Number.parseInt(getStringArg(args, 'frames', '0'), 10);
  const speed = Number.parseFloat(getStringArg(args, 'speed', '1.6'));
  const amplitude = Number.parseFloat(getStringArg(args, 'amplitude', '0.38'));
  const hidden = getBooleanArg(args, 'hidden');
  const debug = getBooleanArg(args, 'debug');
  const syncInterval = Math.max(0, Number.parseInt(getStringArg(args, 'sync-interval', '1'), 10));

  const window = new Win32Window('DXBridge Node.js Triangle', width, height, !hidden, 'DXBridgeNodeTriangle');
  const dxbridge = new DXBridgeLibrary(dllPath || undefined);

  let device = DXBRIDGE_NULL_HANDLE;
  let swapChain = DXBRIDGE_NULL_HANDLE;
  let rtv = DXBRIDGE_NULL_HANDLE;
  let vs = DXBRIDGE_NULL_HANDLE;
  let ps = DXBRIDGE_NULL_HANDLE;
  let inputLayout = DXBRIDGE_NULL_HANDLE;
  let pipeline = DXBRIDGE_NULL_HANDLE;
  let vertexBuffer = DXBRIDGE_NULL_HANDLE;

  try {
    const hwnd = window.create();
    console.log(`Loaded: ${dxbridge.path}`);
    console.log(`Created HWND: 0x${hwnd.toString(16).toUpperCase().padStart(16, '0')}`);

    dxbridge.init(DXB_BACKEND_DX11);
    device = dxbridge.createDevice({
      backend: DXB_BACKEND_DX11,
      adapterIndex: 0,
      flags: debug ? DXB_DEVICE_FLAG_DEBUG : 0
    });
    swapChain = dxbridge.createSwapChain(device, { hwnd, width, height });

    const backBuffer = dxbridge.getBackBuffer(swapChain);
    rtv = dxbridge.createRenderTargetView(device, backBuffer);

    const compileFlags = debug ? DXB_COMPILE_DEBUG : 0;
    vs = dxbridge.compileShader(device, {
      stage: DXB_SHADER_STAGE_VERTEX,
      sourceCode: vertexShaderSource,
      sourceName: 'triangle_vs',
      entryPoint: 'main',
      target: 'vs_5_0',
      compileFlags
    });
    ps = dxbridge.compileShader(device, {
      stage: DXB_SHADER_STAGE_PIXEL,
      sourceCode: pixelShaderSource,
      sourceName: 'triangle_ps',
      entryPoint: 'main',
      target: 'ps_5_0',
      compileFlags
    });

    inputLayout = dxbridge.createInputLayout(
      device,
      [
        { semantic: DXB_SEMANTIC_POSITION, semanticIndex: 0, format: DXB_FORMAT_RG32_FLOAT, inputSlot: 0, byteOffset: 0 },
        { semantic: DXB_SEMANTIC_COLOR, semanticIndex: 0, format: DXB_FORMAT_RGB32_FLOAT, inputSlot: 0, byteOffset: 8 }
      ],
      vs
    );
    pipeline = dxbridge.createPipelineState(device, {
      vs,
      ps,
      inputLayout,
      topology: DXB_PRIM_TRIANGLELIST,
      depthTest: 0,
      depthWrite: 0,
      cullBack: 0,
      wireframe: 0
    });

    vertexBuffer = dxbridge.createBuffer(device, {
      flags: DXB_BUFFER_FLAG_VERTEX | DXB_BUFFER_FLAG_DYNAMIC,
      byteSize: 15 * 4,
      stride: 5 * 4
    });

    const startedAt = performance.now();
    let rendered = 0;
    console.log('Rendering triangle animation. Close the window to exit.');
    while (window.pumpMessages()) {
      const elapsed = (performance.now() - startedAt) / 1000;
      const offsetX = amplitude * Math.sin(elapsed * speed);
      const offsetY = 0.06 * Math.sin(elapsed * (speed * 1.9));
      dxbridge.uploadData(vertexBuffer, makeVertexData(offsetX, offsetY));

      dxbridge.beginFrame(device);
      dxbridge.setRenderTargets(device, rtv);
      dxbridge.clearRenderTarget(rtv, [0.04, 0.05, 0.08, 1.0]);
      dxbridge.setViewport(device, { width, height, minDepth: 0, maxDepth: 1 });
      dxbridge.setPipeline(device, pipeline);
      dxbridge.setVertexBuffer(device, vertexBuffer, 5 * 4, 0);
      dxbridge.draw(device, 3, 0);
      dxbridge.endFrame(device);
      dxbridge.present(swapChain, syncInterval);

      rendered += 1;
      if (frames > 0 && rendered >= frames) {
        window.close();
        break;
      }
    }

    console.log(`Done. Rendered frames: ${rendered}`);
  } finally {
    if (vertexBuffer !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyBuffer(vertexBuffer);
    }
    if (pipeline !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyPipeline(pipeline);
    }
    if (inputLayout !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyInputLayout(inputLayout);
    }
    if (ps !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyShader(ps);
    }
    if (vs !== DXBRIDGE_NULL_HANDLE) {
      dxbridge.destroyShader(vs);
    }
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
