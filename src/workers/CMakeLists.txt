# Workers 模块

add_library(EasyKiConverterWorkers STATIC
    NetworkWorker.cpp
    FetchWorker.cpp
    ProcessWorker.cpp
    WriteWorker.cpp
    WriteWorkerDebugExporter.cpp
    WriteWorkerDebugExporter.h
    MediaFetchWorker.cpp
)

target_include_directories(EasyKiConverterWorkers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterWorkers
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        EasyKiConverterModels
        EasyKiConverterEasyeda
        EasyKiConverterKicad
        EasyKiConverterIR
        EasyKiConverterUtils
        EasyKiConverterSafeUtils # 新增链接
)
