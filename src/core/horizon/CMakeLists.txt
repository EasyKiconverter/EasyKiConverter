add_library(EasyKiConverterHorizon STATIC
    ExporterHorizonLibrary.h
    ExporterHorizonLibrary.cpp
    HorizonUuid.h
    HorizonUnits.h
)

target_include_directories(EasyKiConverterHorizon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterHorizon
    PUBLIC
        EasyKiConverterIR
        EasyKiConverterUtils
        Qt${QT_VERSION_MAJOR}::Core
)
