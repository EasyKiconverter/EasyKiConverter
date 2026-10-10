# EasyKiConverter Utils 模块

# 查找 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Core)

# 1. 日志系统库
add_library(EasyKiConverterLogging STATIC
    # 日志核心
    logging/Logger.cpp
    logging/PatternFormatter.cpp
    
    # 输出器
    logging/ConsoleAppender.cpp
    logging/FileAppender.cpp
    
    # 适配器
    logging/QtLogAdapter.cpp
)

# 设置目标属性
set_target_properties(EasyKiConverterLogging PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# 链接 Qt 库
target_link_libraries(EasyKiConverterLogging
    PUBLIC
        Qt6::Core
)

# 包含目录
target_include_directories(EasyKiConverterLogging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译选项
if(MSVC)
    target_compile_options(EasyKiConverterLogging PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterLogging PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 导出目标
add_library(EasyKiConverter::Logging ALIAS EasyKiConverterLogging)


# 2. 通用工具库 (PathSecurity 等)
add_library(EasyKiConverterSafeUtils STATIC
    PathSecurity.cpp
    PathSecurity.h
    FileUtils.cpp
    FileUtils.h
    BoundedThreadSafeQueue.h
    CommandLineParser.cpp
    CommandLineParser.h
)

# 3. CLI 模块库
add_library(EasyKiConverterCli STATIC
    cli/CliContext.cpp
    cli/CliContext.h
    cli/CliPrinter.cpp
    cli/CliPrinter.h
    cli/FileReader.cpp
    cli/FileReader.h
    cli/BaseConverter.cpp
    cli/BaseConverter.h
    cli/CliExportWaiter.cpp
    cli/CliExportWaiter.h
    cli/BomConverter.cpp
    cli/BomConverter.h
    cli/ComponentConverter.cpp
    cli/ComponentConverter.h
    cli/BatchConverter.cpp
    cli/BatchConverter.h
    cli/CliConverter.cpp
    cli/CliConverter.h
    cli/CompletionGenerator.cpp
    cli/CompletionGenerator.h
)

set_target_properties(EasyKiConverterSafeUtils PROPERTIES
    AUTOMOC ON
)

target_link_libraries(EasyKiConverterSafeUtils
    PUBLIC
        Qt6::Core
        Qt6::Gui
        EasyKiConverterLogging  # 工具库可能需要打日志
)

target_include_directories(EasyKiConverterSafeUtils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(EasyKiConverterSafeUtils PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterSafeUtils PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(EasyKiConverter::SafeUtils ALIAS EasyKiConverterSafeUtils)

# CLI 模块库配置
set_target_properties(EasyKiConverterCli PROPERTIES
    AUTOMOC ON
)

# 查找 Qt 网络组件
find_package(Qt6 REQUIRED COMPONENTS Network)

target_link_libraries(EasyKiConverterCli
    PUBLIC
        Qt6::Core
        Qt6::Network
        EasyKiConverterSafeUtils
        EasyKiConverterLogging
        EasyKiConverterServices
)

target_include_directories(EasyKiConverterCli
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(MSVC)
    target_compile_options(EasyKiConverterCli PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterCli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(EasyKiConverter::Cli ALIAS EasyKiConverterCli)
