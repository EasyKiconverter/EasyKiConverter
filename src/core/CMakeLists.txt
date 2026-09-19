# Core 模块

add_subdirectory(easyeda)
add_subdirectory(ir)
add_subdirectory(kicad)
add_subdirectory(network)
add_subdirectory(parser)
add_subdirectory(utils)
add_subdirectory(altium)
add_subdirectory(cadstar)
add_subdirectory(xpedition)

add_library(EasyKiConverterCore STATIC)

target_sources(EasyKiConverterCore
    PRIVATE
        LanguageManager.cpp
        LanguageManager.h
        ExporterFactory.cpp
        ExporterFactory.h
        interfaces/ISymbolExporter.h
        interfaces/IFootprintExporter.h
        interfaces/IModel3DExporter.h
)

target_link_libraries(EasyKiConverterCore
    PUBLIC
        EasyKiConverterEasyeda
        EasyKiConverterIR
        EasyKiConverterKicad
        EasyKiConverterXpedition
        EasyKiConverterNetwork
        EasyKiConverterParser
        EasyKiConverterUtils
        EasyKiConverterModels
        EasyKiConverterAltium
        EasyKiConverterCadstar
)

target_include_directories(EasyKiConverterCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_BINARY_DIR}/generated
)
