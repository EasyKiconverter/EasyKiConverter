# Cadstar ASCII 库解析与统一 IR 适配模块。

add_library(EasyKiConverterCadstar STATIC
    CadstarAdapter.h
    CadstarAdapter.cpp
)

target_include_directories(EasyKiConverterCadstar
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterCadstar
    PUBLIC
        Qt6::Core
        EasyKiConverterIR
        EasyKiConverterParser
)
