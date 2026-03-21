#![cfg(windows)]

use dxbridge_rust_examples::cli::{AnyResult, CommandLine};
use dxbridge_rust_examples::dxbridge::{
    versioned_input_element, DxBridge, DxbBuffer, DxbDevice, DxbInputLayout, DxbPipeline, DxbRtv,
    DxbShader, DxbSwapChain, DxbViewport, DXB_BACKEND_DX11, DXB_BUFFER_FLAG_DYNAMIC,
    DXB_BUFFER_FLAG_VERTEX, DXB_COMPILE_DEBUG, DXB_DEVICE_FLAG_DEBUG, DXB_FORMAT_RG32_FLOAT,
    DXB_FORMAT_RGB32_FLOAT, DXB_FORMAT_RGBA8_UNORM, DXB_NULL_HANDLE, DXB_PRIM_TRIANGLELIST,
    DXB_SEMANTIC_COLOR, DXB_SEMANTIC_POSITION, DXB_SHADER_STAGE_PIXEL, DXB_SHADER_STAGE_VERTEX,
};
use dxbridge_rust_examples::win32::Win32Window;

const VERTEX_SHADER_SOURCE: &str = r#"
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
"#;

const PIXEL_SHADER_SOURCE: &str = r#"
struct PSInput {
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

float4 main(PSInput p) : SV_TARGET {
    return float4(p.col, 1.0f);
}
"#;

fn main() {
    if let Err(err) = run() {
        eprintln!("{}", err);
        std::process::exit(1);
    }
}

fn run() -> AnyResult<()> {
    let cli = CommandLine::from_env()?;
    if cli.help_requested {
        println!("Usage: example05_dx11_moving_triangle [--dll PATH] [--width 960] [--height 540] [--frames 0] [--speed 1.6] [--amplitude 0.38] [--hidden] [--debug] [--sync-interval 1]");
        return Ok(());
    }

    let width = cli.get_u32("--width", 960)?;
    let height = cli.get_u32("--height", 540)?;
    let frames = cli.get_u32("--frames", 0)?;
    let speed = cli.get_f64("--speed", 1.6)?;
    let amplitude = cli.get_f64("--amplitude", 0.38)?;
    let sync_interval = cli.get_u32("--sync-interval", 1)?;

    let mut window = Win32Window::new(
        "DXBridge Rust Moving Triangle",
        width as i32,
        height as i32,
        !cli.has_flag("--hidden"),
        "DXBridgeRustMovingTriangle",
    )?;
    let bridge = DxBridge::load(cli.get_optional_string("--dll").as_deref())?;

    let mut initialized = false;
    let mut device: DxbDevice = DXB_NULL_HANDLE;
    let mut swap_chain: DxbSwapChain = DXB_NULL_HANDLE;
    let mut rtv: DxbRtv = DXB_NULL_HANDLE;
    let mut vertex_shader: DxbShader = DXB_NULL_HANDLE;
    let mut pixel_shader: DxbShader = DXB_NULL_HANDLE;
    let mut input_layout: DxbInputLayout = DXB_NULL_HANDLE;
    let mut pipeline: DxbPipeline = DXB_NULL_HANDLE;
    let mut vertex_buffer: DxbBuffer = DXB_NULL_HANDLE;

    let result = (|| -> AnyResult<()> {
        let hwnd = window.create()?;

        println!("Loaded: {}", bridge.path.display());
        println!("Window: {}", window.title());
        println!("Created HWND: 0x{:016X}", hwnd as usize);

        bridge.init(DXB_BACKEND_DX11)?;
        initialized = true;

        let flags = if cli.has_flag("--debug") {
            DXB_DEVICE_FLAG_DEBUG
        } else {
            0
        };
        let compile_flags = if cli.has_flag("--debug") {
            DXB_COMPILE_DEBUG
        } else {
            0
        };

        device = bridge.create_device(DXB_BACKEND_DX11, 0, flags)?;
        swap_chain = bridge.create_swap_chain(
            device,
            hwnd as *mut _,
            width,
            height,
            DXB_FORMAT_RGBA8_UNORM,
            2,
            0,
        )?;
        let back_buffer = bridge.get_back_buffer(swap_chain)?;
        rtv = bridge.create_render_target_view(device, back_buffer)?;

        vertex_shader = bridge.compile_shader(
            device,
            DXB_SHADER_STAGE_VERTEX,
            VERTEX_SHADER_SOURCE,
            "moving_triangle_vs",
            "main",
            "vs_5_0",
            compile_flags,
        )?;
        pixel_shader = bridge.compile_shader(
            device,
            DXB_SHADER_STAGE_PIXEL,
            PIXEL_SHADER_SOURCE,
            "moving_triangle_ps",
            "main",
            "ps_5_0",
            compile_flags,
        )?;

        let elements = [
            versioned_input_element(DXB_SEMANTIC_POSITION, 0, DXB_FORMAT_RG32_FLOAT, 0, 0),
            versioned_input_element(DXB_SEMANTIC_COLOR, 0, DXB_FORMAT_RGB32_FLOAT, 0, 8),
        ];
        input_layout = bridge.create_input_layout(device, &elements, vertex_shader)?;
        pipeline = bridge.create_pipeline_state(
            device,
            vertex_shader,
            pixel_shader,
            input_layout,
            DXB_PRIM_TRIANGLELIST,
            0,
            0,
            0,
            0,
        )?;
        vertex_buffer = bridge.create_buffer(
            device,
            DXB_BUFFER_FLAG_VERTEX | DXB_BUFFER_FLAG_DYNAMIC,
            15 * 4,
            5 * 4,
        )?;

        let viewport = DxbViewport {
            x: 0.0,
            y: 0.0,
            width: width as f32,
            height: height as f32,
            min_depth: 0.0,
            max_depth: 1.0,
        };

        let start = std::time::Instant::now();
        let mut rendered = 0u32;
        println!("Rendering triangle animation. Close the window to exit.");
        while window.pump_messages() {
            let elapsed = start.elapsed().as_secs_f64();
            let offset_x = (amplitude * (elapsed * speed).sin()) as f32;
            let offset_y = (0.06 * (elapsed * speed * 1.9).sin()) as f32;
            let vertices = make_vertex_data(offset_x, offset_y);

            bridge.upload_floats(vertex_buffer, &vertices)?;
            bridge.begin_frame(device)?;
            bridge.set_render_targets(device, rtv, DXB_NULL_HANDLE)?;
            bridge.clear_render_target(rtv, [0.04, 0.05, 0.08, 1.0])?;
            bridge.set_viewport(device, viewport)?;
            bridge.set_pipeline(device, pipeline)?;
            bridge.set_vertex_buffer(device, vertex_buffer, 5 * 4, 0)?;
            bridge.draw(device, 3, 0)?;
            bridge.end_frame(device)?;
            bridge.present(swap_chain, sync_interval)?;

            rendered += 1;
            if frames > 0 && rendered >= frames {
                window.close();
                break;
            }
        }

        println!("Done. Rendered frames: {}", rendered);
        Ok(())
    })();

    if vertex_buffer != DXB_NULL_HANDLE {
        bridge.destroy_buffer(vertex_buffer);
    }
    if pipeline != DXB_NULL_HANDLE {
        bridge.destroy_pipeline(pipeline);
    }
    if input_layout != DXB_NULL_HANDLE {
        bridge.destroy_input_layout(input_layout);
    }
    if pixel_shader != DXB_NULL_HANDLE {
        bridge.destroy_shader(pixel_shader);
    }
    if vertex_shader != DXB_NULL_HANDLE {
        bridge.destroy_shader(vertex_shader);
    }
    if rtv != DXB_NULL_HANDLE {
        bridge.destroy_render_target_view(rtv);
    }
    if swap_chain != DXB_NULL_HANDLE {
        bridge.destroy_swap_chain(swap_chain);
    }
    if device != DXB_NULL_HANDLE {
        bridge.destroy_device(device);
    }
    if initialized {
        bridge.shutdown();
    }

    result
}

fn make_vertex_data(offset_x: f32, offset_y: f32) -> [f32; 15] {
    [
        0.0 + offset_x,
        0.52 + offset_y,
        1.0,
        0.1,
        0.1,
        -0.42 + offset_x,
        -0.38 + offset_y,
        0.1,
        1.0,
        0.1,
        0.42 + offset_x,
        -0.38 + offset_y,
        0.1,
        0.3,
        1.0,
    ]
}
