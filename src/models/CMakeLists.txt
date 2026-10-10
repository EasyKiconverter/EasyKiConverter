# Models 模块

add_library(EasyKiConverterModels STATIC
    ComponentData.cpp
    SymbolData.cpp
    SymbolDataSerializer.cpp
    SymbolDataShapeCompatibility.cpp
    SymbolPinSerializer.cpp
    SymbolShapeSerializer.cpp
    FootprintData.cpp
    FootprintDataSerializer.cpp
    FootprintDataShapeCompatibility.cpp
    FootprintShapeSerializer.cpp
    Model3DData.cpp
    ComponentListItemData.cpp
)

target_include_directories(EasyKiConverterModels
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(EasyKiConverterModels
    PUBLIC
        Qt6::Core
        Qt6::Gui
)
