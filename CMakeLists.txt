cmake_minimum_required(VERSION 3.20)
project(dxbridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CompilerFlags.cmake)
include(cmake/FindWindowsSDK.cmake)

# ---------------------------------------------------------------------------
# dxbridge_common (internal infrastructure — no GPU dependency)
# ---------------------------------------------------------------------------
add_library(dxbridge_common STATIC
    src/common/handle_table.cpp
    src/common/error.cpp
    src/common/log.cpp
)
target_include_directories(dxbridge_common PUBLIC include src)

# ---------------------------------------------------------------------------
# dxbridge_dx11 (DX11 backend)
# ---------------------------------------------------------------------------
add_library(dxbridge_dx11 STATIC
    src/dx11/device_dx11.cpp
    src/dx11/swapchain_dx11.cpp
    src/dx11/buffer_dx11.cpp
    src/dx11/shader_dx11.cpp
    src/dx11/pipeline_dx11.cpp
    src/dx11/render_dx11.cpp
)
target_link_libraries(dxbridge_dx11 PUBLIC dxbridge_common d3d11 dxgi d3dcompiler)
target_include_directories(dxbridge_dx11 PRIVATE src)

# ---------------------------------------------------------------------------
# dxbridge_dx12 (DX12 backend)
# ---------------------------------------------------------------------------
add_library(dxbridge_dx12 STATIC
    src/dx12/device_dx12.cpp
    src/dx12/swapchain_dx12.cpp
    src/dx12/buffer_dx12.cpp
    src/dx12/shader_dx12.cpp
    src/dx12/pipeline_dx12.cpp
    src/dx12/render_dx12.cpp
)
target_link_libraries(dxbridge_dx12 PUBLIC dxbridge_common d3d12 dxgi d3dcompiler)
target_include_directories(dxbridge_dx12 PRIVATE src)

# ---------------------------------------------------------------------------
# dxbridge (the actual DLL)
# ---------------------------------------------------------------------------
add_library(dxbridge SHARED
    src/dxbridge.cpp
)
target_compile_definitions(dxbridge PRIVATE DXBRIDGE_EXPORTS)
target_link_libraries(dxbridge PRIVATE dxbridge_dx11 dxbridge_dx12)
set_target_properties(dxbridge PROPERTIES
    OUTPUT_NAME "dxbridge"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
if(MSVC)
    set_target_properties(dxbridge PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/dxbridge.def"
    )
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
add_subdirectory(examples)
