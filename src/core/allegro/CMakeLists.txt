# Allegro 符号、封装和三维语义 Import Package 导出模块

add_library(EasyKiConverterAllegro STATIC
    AllegroFootprintModel.h
    AllegroLayerMapper.h
    AllegroLayerMapper.cpp
    ExporterAllegroFootprint.h
    ExporterAllegroFootprint.cpp
)

target_include_directories(EasyKiConverterAllegro
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterAllegro
    PUBLIC
        Qt6::Core
        EasyKiConverterIR
)
