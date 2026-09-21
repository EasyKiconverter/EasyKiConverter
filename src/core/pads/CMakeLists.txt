# PADS Parts Library ASCII 封装导出模块

add_library(EasyKiConverterPads STATIC
    ExporterPadsFootprint.h
    ExporterPadsFootprint.cpp
)

target_include_directories(EasyKiConverterPads
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterPads
    PUBLIC
        EasyKiConverterIR
        EasyKiConverterUtils
)
