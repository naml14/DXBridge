if(MSVC)
    add_compile_options(/W4 /WX /std:c++17 /utf-8 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    # Per-configuration flags
    add_compile_options("$<$<CONFIG:Debug>:/Od;/Zi;/D_DEBUG>")
    add_compile_options("$<$<CONFIG:Release>:/O2;/DNDEBUG>")
endif()
