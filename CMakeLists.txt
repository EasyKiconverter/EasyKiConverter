cmake_minimum_required(VERSION 3.16)

# 从 CI 环境动态获取版本号，如果没有定义则使用默认版本
if(NOT DEFINED VERSION_FROM_CI)
    set(VERSION_FROM_CI "3.1.13")
    message(STATUS "VERSION_FROM_CI not defined, using default: ${VERSION_FROM_CI}")
else()
    message(STATUS "VERSION_FROM_CI defined: ${VERSION_FROM_CI}")
endif()

# 检查版本号格式 (X.Y.Z) 并提取有效的语义化版本号
# 匹配格式：数字.数字.数字（可能包含后缀，如 3.0.1_test_release）
if(VERSION_FROM_CI MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)")
    # 提取纯数字版本号（去掉后缀）
    set(PROJECT_VERSION_SANITIZED ${CMAKE_MATCH_0})
    message(STATUS "Extracted semantic version: ${PROJECT_VERSION_SANITIZED}")
elseif(VERSION_FROM_CI MATCHES "^([0-9]+\\.[0-9]+)")
    # 如果只有 X.Y 格式，添加 .0 补全
    set(PROJECT_VERSION_SANITIZED "${CMAKE_MATCH_0}.0")
    message(STATUS "Extracted semantic version (with .0): ${PROJECT_VERSION_SANITIZED}")
else()
    # 如果版本号不是数字格式 (例如 "dev")，使用安全的默认值
    set(PROJECT_VERSION_SANITIZED "0.0.0")
    message(STATUS "Version '${VERSION_FROM_CI}' is not semantic, defaulting project version to ${PROJECT_VERSION_SANITIZED}")
endif()

project(EasyKiconverter_Cpp_Version VERSION ${PROJECT_VERSION_SANITIZED} LANGUAGES CXX)

# 生成 compile_commands.json 用于 VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 抑制 Qt 私有头文件警告（QXlsx 使用 QtGuiPrivate，需要此设置）
set(QT_NO_PRIVATE_MODULE_WARNING ON CACHE BOOL "" FORCE)

# 显式设置版本号变量，确保 configure_file 能正确替换
# 如果补丁号未定义或为空，默认设置为 0
if(NOT DEFINED PROJECT_VERSION_PATCH OR "${PROJECT_VERSION_PATCH}" STREQUAL "")
    set(PROJECT_VERSION_PATCH 0)
    message(STATUS "PROJECT_VERSION_PATCH not defined or empty, setting to 0")
endif()

set(PROJECT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(PROJECT_VERSION ${PROJECT_VERSION})

# 调试信息：打印版本号
message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Major: ${PROJECT_VERSION_MAJOR}, Minor: ${PROJECT_VERSION_MINOR}, Patch: ${PROJECT_VERSION_PATCH}")

# 生成版本头文件，统一版本来源
configure_file(
    ${CMAKE_SOURCE_DIR}/src/core/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h
    @ONLY
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用多线程编译
if(MSVC)
    # MSVC 使用 /MP 选项，自动使用最大可用线程
    # /utf-8 确保编译器将源文件视为 UTF-8 编码（无需 BOM）
    add_compile_options(/MP /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 使用 -j 选项，但需要通过 make 的 -j 参数来控制
    # 这里不添加编译选项，而是在构建时使用 make -j
    message(STATUS "GCC compiler detected. Use 'make -j' or 'cmake --build . --parallel' for parallel compilation")
endif()

# 调试选项：启用符号和封装解析数据导出
option(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT "Enable debug export for symbol and footprint data" ON)

if(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
    message(STATUS "Symbol and footprint debug export is ENABLED")
    add_compile_definitions(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
else()
    message(STATUS "Symbol and footprint debug export is DISABLED")
endif()

# 覆盖率支持
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        message(STATUS "Coverage reporting is ENABLED")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage is only supported for GCC/MinGW")
    endif()
endif()

# CI 环境检测
if(DEFINED ENV{CI})
    message(STATUS "Running in CI environment")
    add_compile_definitions(CI_ENVIRONMENT=1)
else()
    message(STATUS "Running in local environment")
    add_compile_definitions(CI_ENVIRONMENT=0)
endif()

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找 Qt 6 组件
find_package(Qt6 REQUIRED COMPONENTS Quick Network Core Gui GuiPrivate Widgets QuickControls2 Concurrent LinguistTools)
find_package(Qt6 REQUIRED COMPONENTS Gui) # 再次确保 Gui

# 尝试查找 zlib 库
# 在 Windows + MinGW 环境下，Qt 的 zlib 库通常在 Qt 安装目录中
# 在 CI 环境中，vcpkg 会提供 zlib
find_package(ZLIB QUIET)

# 如果找不到标准的 zlib，使用 Qt 的 zlib 库
if(NOT ZLIB_FOUND)
    # 使用 Qt 的 zlib 库（在 MinGW 环境下通常可用）
    message(STATUS "Using Qt's zlib library")
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIR "")
    set(ZLIB_LIBRARY z)
    set(ZLIB_USE_QT TRUE)
else()
    # 找到了 zlib（可能是 vcpkg 提供的）
    message(STATUS "Found zlib: ${ZLIB_INCLUDE_DIRS}")
    message(STATUS "ZLIB libraries: ${ZLIB_LIBRARIES}")
    set(ZLIB_USE_QT FALSE)
endif()

message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_LIBRARY: ${ZLIB_LIBRARY}")
message(STATUS "ZLIB_USE_QT: ${ZLIB_USE_QT}")

qt_standard_project_setup(REQUIRES 6.6)

# Qt 6.5+ 要求 QML 模块为包含额外 QML 子目录的路径提供 qmldir。
# 显式启用新策略，尽早发现模块资源拓扑问题。
if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

# 添加 QXlsx 库（用于 Excel 文件解析，必须依赖）
include(FetchContent)

# 配置 FetchContent 选项
set(FETCHCONTENT_QUIET OFF CACHE BOOL "")
set(FETCHCONTENT_UPDATES_DISCONNECTED OFF CACHE BOOL "")
set(FETCHCONTENT_FULLY_DISCONNECTED OFF CACHE BOOL "")

# 首先尝试查找已安装的 QXlsx（Flatpak 或系统安装）
find_package(QXlsxQt6 QUIET)
if(QXlsxQt6_FOUND)
    message(STATUS "Found system QXlsxQt6, using it")
    if(NOT TARGET QXlsx::QXlsx)
        add_library(QXlsx INTERFACE IMPORTED)
        add_library(QXlsx::QXlsx ALIAS QXlsx)
        target_link_libraries(QXlsx INTERFACE QXlsxQt6)
    endif()
elseif(DEFINED ENV{QXLSX_ROOT})
    message(STATUS "Using local QXlsx source from: $ENV{QXLSX_ROOT}")
    add_subdirectory($ENV{QXLSX_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/qxlsx)
    message(STATUS "QXlsx target created: QXlsx::QXlsx exists: $<TARGET_EXISTS:QXlsx::QXlsx>")
else()
    # 尝试使用 find_package (Flatpak 构建)
    message(STATUS "FLATPAK_BUILD=${FLATPAK_BUILD}, CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
    find_package(QXlsxQt6 QUIET)
    if(QXlsxQt6_FOUND)
        # 创建 QXlsx::QXlsx 别名以兼容现有代码
        message(STATUS "Found QXlsxQt6, creating QXlsx::QXlsx alias")
        if(NOT TARGET QXlsx::QXlsx)
            add_library(QXlsx INTERFACE IMPORTED)
            add_library(QXlsx::QXlsx ALIAS QXlsx)
            target_link_libraries(QXlsx INTERFACE QXlsxQt6)
        endif()
    else()
        message(STATUS "QXlsxQt6 not found, downloading from GitHub")
        # 从 GitHub 下载
        FetchContent_Declare(
            QXlsx
            GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
            GIT_TAG        9f545935a1effa0144ba76598e95a7597210553a
            SOURCE_SUBDIR  QXlsx
        )
        FetchContent_MakeAvailable(QXlsx)
    endif()
endif()

# 添加源码模块（包含所有子模块的 CMakeLists.txt）
add_subdirectory(src)

# 添加测试模块（可选构建）
option(EASYKICONVERTER_BUILD_TESTS "Build tests for EasyKiConverter" OFF)
if(EASYKICONVERTER_BUILD_TESTS)
    message(STATUS "Tests build is ENABLED")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Tests build is DISABLED (default)")
endif()

# QML 文件列表
set(QML_FILES
    src/ui/qml/Main.qml
    src/ui/qml/MainWindow.qml
    src/ui/qml/components/Card.qml
    src/ui/qml/components/ComponentListItem.qml
    src/ui/qml/components/ListItemInteraction.qml
    src/ui/qml/components/ResultListItem.qml
    src/ui/qml/components/Icon.qml
    src/ui/qml/components/ModernButton.qml
    src/ui/qml/components/StatItem.qml
    src/ui/qml/components/StepItem.qml
    src/ui/qml/components/TitleBar.qml
    src/ui/qml/components/HeaderSection.qml
    src/ui/qml/components/ComponentInputCard.qml
    src/ui/qml/components/BomImportCard.qml
    src/ui/qml/components/ComponentListCard.qml
    src/ui/qml/components/ComponentToolbar.qml
    src/ui/qml/components/ComponentListView.qml
    src/ui/qml/components/ComponentPreviewPopup.qml
    src/ui/qml/components/ExportSettingsBaseCard.qml
    src/ui/qml/components/KiCadSettingsCard.qml
    src/ui/qml/components/AltiumSettingsCard.qml
    src/ui/qml/components/AllegroSettingsCard.qml
    src/ui/qml/components/StyledCheckBox.qml
    src/ui/qml/components/SettingsSectionHeader.qml
    src/ui/qml/components/ExportProgressCard.qml
    src/ui/qml/components/ExportResultsCard.qml
    src/ui/qml/components/ExportStatisticsCard.qml
    src/ui/qml/components/ExportButtonsSection.qml
    src/ui/qml/components/SidebarExportControls.qml
    src/ui/qml/components/UpdateBanner.qml
    src/ui/qml/components/UpdateDialog.qml
    src/ui/qml/components/WindowController.qml
    src/ui/qml/components/WindowPersistenceManager.qml
    src/ui/qml/components/WindowStartupManager.qml
    src/ui/qml/components/ConfirmDialog.qml
    src/ui/qml/components/DescriptionEditDialog.qml
    src/ui/qml/components/WindowResizeHandles.qml
    src/ui/qml/components/ExitDialog.qml
    src/ui/qml/components/SliderDialogBase.qml
    src/ui/qml/components/SidebarPanel.qml
    src/ui/qml/components/SidebarSettingsView.qml
    src/ui/qml/components/SidebarSection.qml
    src/ui/qml/components/SidebarToggleRow.qml
    src/ui/qml/components/SidebarTextField.qml
    src/ui/qml/components/SidebarSegmentedControl.qml
    src/ui/qml/styles/AppStyle.qml
    src/ui/qml/styles/ResponsiveHelper.qml
)

# Windows 资源文件配置
if(WIN32)
    # 生成 Windows 资源文件
    configure_file(resources/windows_meta.rc.in ${CMAKE_BINARY_DIR}/windows_meta.rc)
endif()

# 创建可执行文件
qt_add_executable(appEasyKiconverter_Cpp_Version
    src/main.cpp
)

# 添加 Windows 资源文件
if(WIN32)
    target_sources(appEasyKiconverter_Cpp_Version PRIVATE ${CMAKE_BINARY_DIR}/windows_meta.rc)
endif()

# 添加编译定义
if(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
    message(STATUS "Symbol and footprint debug export is ENABLED")
    target_compile_definitions(appEasyKiconverter_Cpp_Version PRIVATE ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
else()
    message(STATUS "Symbol and footprint debug export is DISABLED")
endif()

# ============================================
# 翻译文件 (Qt 6.6+ LinguistTools)
# 使用 qt_add_translations 自动发现 .ts 文件并生成 .qm 文件
# ============================================
set(TS_FILES
    resources/translations/translations_easykiconverter_en.ts
    resources/translations/translations_easykiconverter_zh_CN.ts
)

qt_add_translations(appEasyKiconverter_Cpp_Version
    TS_FILES ${TS_FILES}
)

# 添加 QML 模块
qt_add_qml_module(appEasyKiconverter_Cpp_Version
    URI EasyKiconverter_Cpp_Version
    VERSION 3.1
    QML_FILES
        ${QML_FILES}
    RESOURCES
        resources/icons/app_icon.icns
        resources/icons/app_icon.ico
        resources/icons/app_icon.png
        resources/icons/app_icon.svg
        resources/icons/app_icon_dark.icns
        resources/icons/app_icon_dark.ico
        resources/icons/app_icon_dark.png
        resources/icons/app_icon_dark.svg
        resources/icons/hicolor/16x16/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/24x24/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/32x32/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/48x48/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/64x64/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/128x128/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor/512x512/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/16x16/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/24x24/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/32x32/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/48x48/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/64x64/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/128x128/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/256x256/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/hicolor-dark/512x512/apps/io.github.tangsangsimida.easykiconverter.png
        resources/icons/play.svg
        resources/icons/play_black.svg
        resources/icons/folder.svg
        resources/icons/trash.svg
        resources/icons/upload.svg
        resources/icons/add.svg
        resources/icons/minimize.svg
        resources/icons/maximize.svg
        resources/icons/close.svg
        src/resources/export_plugins.json
        resources/icons/loading.svg
        resources/icons/github-mark.svg
        resources/icons/github-mark-white.svg
        resources/icons/github-mark.png
        resources/icons/github-mark-white.png
        resources/icons/Blue_light_bulb.svg
        resources/icons/Grey_light_bulb.svg
        resources/imgs/background.jpg
        src/ui/qml/styles/qmldir
        src/ui/qml/components/qmldir
)

# 控制台模式选项（调试时启用可看到控制台日志）
option(EASYKICONVERTER_CONSOLE_MODE "Build as console application (shows terminal output)" OFF)

# 设置目标属性
set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE $<NOT:$<BOOL:${EASYKICONVERTER_CONSOLE_MODE}>>
    OUTPUT_NAME "easykiconverter"
)

# 链接源码库和 Qt 库
target_link_libraries(appEasyKiconverter_Cpp_Version
    PRIVATE
        EasyKiConverterLib
        EasyKiConverter::Cli
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Gui
        Qt6::Widgets
)

# Windows 内存统计需要 psapi 库 (MSVC)
if(WIN32)
    if(MSVC)
        target_link_libraries(appEasyKiconverter_Cpp_Version PRIVATE psapi)
    endif()
endif()

# 包含目录（用于 main.cpp）
target_include_directories(appEasyKiconverter_Cpp_Version
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
)

# 编译选项
if(MSVC)
    target_compile_options(appEasyKiconverter_Cpp_Version PRIVATE /W4 /wd4702)
else()
    target_compile_options(appEasyKiconverter_Cpp_Version PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
include(GNUInstallDirs)
install(TARGETS appEasyKiconverter_Cpp_Version
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

# 安装图标（浅色主题）
install(DIRECTORY resources/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
    FILES_MATCHING PATTERN "*.png"
)

# 安装 desktop 文件
install(FILES deploy/linux/io.github.tangsangsimida.easykiconverter.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# 安装 license 文件
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/io.github.tangsangsimida.easykiconverter
)

# 安装 AppStream metainfo 文件
install(FILES deploy/metainfo/io.github.tangsangsimida.easykiconverter.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

# 安装 AppStream appdata 文件（Flathub 要求）
install(FILES deploy/metainfo/io.github.tangsangsimida.easykiconverter.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/appdata
    RENAME io.github.tangsangsimida.easykiconverter.appdata.xml
)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt version: ${Qt6_VERSION}")
