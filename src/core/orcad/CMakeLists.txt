add_library(EasyKiConverterOrcad STATIC
    ExporterOrcadSymbol.h
    ExporterOrcadSymbol.cpp
)

target_include_directories(EasyKiConverterOrcad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterOrcad
    PUBLIC
        Qt6::Core
        EasyKiConverterIR
)
