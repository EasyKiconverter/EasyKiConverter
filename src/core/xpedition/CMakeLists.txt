# Xpedition ASCII 库导出模块

add_library(EasyKiConverterXpedition STATIC
    XpeditionHkpAdapter.h
    XpeditionHkpAdapter.cpp
    XpeditionZipWriter.h
    XpeditionZipWriter.cpp
    ExporterXpeditionSymbol.h
    ExporterXpeditionSymbol.cpp
    XpeditionSymbolAdapter.h
    XpeditionSymbolAdapter.cpp
    ExporterXpeditionFootprint.h
    ExporterXpeditionFootprint.cpp
)

target_include_directories(EasyKiConverterXpedition
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterXpedition
    PUBLIC
        Qt6::Core
        EasyKiConverterIR
        EasyKiConverterParser
)
