# 通用文本格式解析基础设施。

add_library(EasyKiConverterParser STATIC
    ParseDiagnostics.h
    ParseDiagnostics.cpp
    GeometryTransforms.h
    GeometryTransforms.cpp
    XpeditionHkpReader.h
    XpeditionHkpReader.cpp
    XpeditionHkpModel.h
    XpeditionHkpModel.cpp
    XpeditionSymbolModel.h
    XpeditionSymbolModel.cpp
    CadstarModel.h
    CadstarModel.cpp
    TextParsers.h
    TextParsers.cpp
)

target_include_directories(EasyKiConverterParser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterParser
    PUBLIC
        Qt6::Core
)
