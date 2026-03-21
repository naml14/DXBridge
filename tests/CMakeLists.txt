function(dxbridge_add_test test_name)
    set(options)
    set(one_value_args)
    set(multi_value_args LABELS)
    cmake_parse_arguments(DXB_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_test(NAME ${test_name} COMMAND ${test_name})

    if(DXB_TEST_LABELS)
        set_tests_properties(${test_name} PROPERTIES LABELS "${DXB_TEST_LABELS}")
    endif()
endfunction()

add_executable(test_handle_table unit/test_handle_table.cpp)
target_link_libraries(test_handle_table PRIVATE dxbridge_common)
dxbridge_add_test(test_handle_table LABELS unit core smoke)

add_executable(test_error_mapping unit/test_error_mapping.cpp)
target_link_libraries(test_error_mapping PRIVATE dxbridge_common)
dxbridge_add_test(test_error_mapping LABELS unit core smoke)

add_executable(test_dx11_validation unit/test_dx11_validation.cpp)
target_link_libraries(test_dx11_validation PRIVATE dxbridge_dx11 dxbridge_common user32)
target_include_directories(test_dx11_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
dxbridge_add_test(test_dx11_validation LABELS unit hardening dx11 smoke)

add_executable(test_dx12_validation unit/test_dx12_validation.cpp)
target_link_libraries(test_dx12_validation PRIVATE dxbridge_dx12 dxbridge_common user32)
target_include_directories(test_dx12_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
dxbridge_add_test(test_dx12_validation LABELS unit hardening dx12 smoke)

# ---------------------------------------------------------------------------
# Integration tests (require dxbridge.dll — copy it next to the executable)
# ---------------------------------------------------------------------------

add_executable(test_headless_triangle_dx11
    integration/headless_triangle_dx11.cpp
)
target_link_libraries(test_headless_triangle_dx11 PRIVATE dxbridge d3d11 d3dcompiler)
target_include_directories(test_headless_triangle_dx11 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
dxbridge_add_test(test_headless_triangle_dx11 LABELS integration hardening dx11)

add_custom_command(TARGET test_headless_triangle_dx11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dxbridge>
        $<TARGET_FILE_DIR:test_headless_triangle_dx11>
)

add_executable(test_headless_triangle_dx12
    integration/headless_triangle_dx12.cpp
)
target_link_libraries(test_headless_triangle_dx12 PRIVATE dxbridge d3d12 d3dcompiler)
target_include_directories(test_headless_triangle_dx12 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
dxbridge_add_test(test_headless_triangle_dx12 LABELS integration hardening dx12)

add_custom_command(TARGET test_headless_triangle_dx12 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dxbridge>
        $<TARGET_FILE_DIR:test_headless_triangle_dx12>
)

add_custom_target(check_smoke
    COMMAND ${CMAKE_CTEST_COMMAND}
        --test-dir ${CMAKE_BINARY_DIR}
        -C $<CONFIG>
        -L smoke
        --output-on-failure
    USES_TERMINAL
    COMMENT "Running dxbridge smoke tests"
)
