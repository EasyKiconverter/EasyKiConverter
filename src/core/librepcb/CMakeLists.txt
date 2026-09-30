add_library(EasyKiConverterLibrePcb STATIC
    ExporterLibrePcbLibrary.h
    ExporterLibrePcbLibrary.cpp
)

target_include_directories(EasyKiConverterLibrePcb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterLibrePcb
    PUBLIC
        EasyKiConverterIR
        EasyKiConverterUtils
)
