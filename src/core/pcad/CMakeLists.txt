# P-CAD ASCII PCB 专用模型到统一 IR 的适配模块。

add_library(EasyKiConverterPcad STATIC
    PcadAdapter.h
    PcadAdapter.cpp
)

target_include_directories(EasyKiConverterPcad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterPcad
    PUBLIC
        Qt6::Core
        EasyKiConverterIR
        EasyKiConverterParser
)
