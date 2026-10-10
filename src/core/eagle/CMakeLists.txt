add_library(EasyKiConverterEagle STATIC
    ExporterEagleFootprint.h
    ExporterEagleFootprint.cpp
)

target_include_directories(EasyKiConverterEagle
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterEagle
    PUBLIC
        EasyKiConverterIR
        EasyKiConverterUtils
)
