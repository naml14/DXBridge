# ---------------------------------------------------------------------------
# examples/hello_triangle — Win32 windowed Hello Triangle
# ---------------------------------------------------------------------------

add_executable(hello_triangle WIN32
    hello_triangle/main_dx11.cpp
)
target_link_libraries(hello_triangle PRIVATE dxbridge d3d11 d3dcompiler)
target_include_directories(hello_triangle PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_custom_command(TARGET hello_triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dxbridge>
        $<TARGET_FILE_DIR:hello_triangle>
)

add_test(NAME smoke_hello_triangle_dx11
    COMMAND $<TARGET_FILE:hello_triangle> --self-test
)
set_tests_properties(smoke_hello_triangle_dx11 PROPERTIES
    LABELS "smoke;example;dx11"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:hello_triangle>
)

# ---------------------------------------------------------------------------
# examples/hello_triangle — DX12 version
# ---------------------------------------------------------------------------

add_executable(hello_triangle_dx12 WIN32
    hello_triangle/main_dx12.cpp
)
target_link_libraries(hello_triangle_dx12 PRIVATE dxbridge d3d12 d3dcompiler)
target_include_directories(hello_triangle_dx12 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_custom_command(TARGET hello_triangle_dx12 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dxbridge>
        $<TARGET_FILE_DIR:hello_triangle_dx12>
)
