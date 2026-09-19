find_package(Qt6 REQUIRED COMPONENTS Test Gui Network Concurrent)

add_executable(test_easyeda_api
    test_easyeda_api.cpp
    ../common/MockNetworkClient.hpp
)
target_link_libraries(test_easyeda_api PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)

# 确保能找到项目根目录下的包含路径
target_include_directories(test_easyeda_api PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME test_easyeda_api COMMAND test_easyeda_api)

add_executable(test_text_parsers test_text_parsers.cpp)
target_link_libraries(test_text_parsers PRIVATE
    Qt6::Test
    EasyKiConverterParser
)
target_include_directories(test_text_parsers PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_text_parsers COMMAND test_text_parsers)

add_executable(test_models test_models.cpp)
target_link_libraries(test_models PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_models PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_models COMMAND test_models)

add_executable(test_component_cache_service test_component_cache_service.cpp)
target_link_libraries(test_component_cache_service PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_component_cache_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_component_cache_service COMMAND test_component_cache_service)

add_executable(test_cache_pruner test_cache_pruner.cpp)
target_link_libraries(test_cache_pruner PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_cache_pruner PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_cache_pruner COMMAND test_cache_pruner)

add_executable(test_core_utils test_core_utils.cpp)
target_link_libraries(test_core_utils PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_core_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_core_utils COMMAND test_core_utils)

add_executable(test_atomic_file_writer test_atomic_file_writer.cpp)
target_link_libraries(test_atomic_file_writer PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_atomic_file_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_atomic_file_writer COMMAND test_atomic_file_writer)

add_executable(test_altium_ole test_altium_ole.cpp)
target_link_libraries(test_altium_ole PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_altium_ole PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/altium
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_altium_ole COMMAND test_altium_ole)

add_executable(test_temp_file_manager test_temp_file_manager.cpp)
target_link_libraries(test_temp_file_manager PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_temp_file_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_temp_file_manager COMMAND test_temp_file_manager)

add_executable(test_config_service test_config_service.cpp)
target_link_libraries(test_config_service PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_config_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_config_service COMMAND test_config_service)

add_executable(test_component_service test_component_service.cpp)
target_link_libraries(test_component_service PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_component_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
target_compile_definitions(test_component_service PRIVATE EASYKICONVERTER_TESTING_ACCESS)
add_test(NAME test_component_service COMMAND test_component_service)

# 导出阶段测试
add_executable(test_export_type_stage test_export_type_stage.cpp)
target_link_libraries(test_export_type_stage PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_type_stage PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_type_stage COMMAND test_export_type_stage)

add_executable(test_export_progress test_export_progress.cpp)
target_link_libraries(test_export_progress PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_export_progress PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_progress COMMAND test_export_progress)

add_executable(test_fetch_worker
    test_fetch_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/workers/FetchWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ConfigService.cpp
    ../common/MockNetworkClient.hpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_fetch_worker PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterNetwork
    EasyKiConverterModels
    EasyKiConverterUtils
)
target_include_directories(test_fetch_worker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_fetch_worker COMMAND test_fetch_worker)

add_executable(test_process_worker
    test_process_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/workers/ProcessWorker.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_process_worker PRIVATE
    Qt6::Test
    EasyKiConverterEasyeda
    EasyKiConverterNetwork
    EasyKiConverterModels
    EasyKiConverterUtils
)
target_include_directories(test_process_worker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_process_worker COMMAND test_process_worker)

add_executable(test_write_worker
    test_write_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/workers/WriteWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/workers/WriteWorkerDebugExporter.cpp
)
target_link_libraries(test_write_worker PRIVATE
    Qt6::Test
    Qt6::Concurrent
    EasyKiConverterKicad
    EasyKiConverterModels
    EasyKiConverterUtils
    EasyKiConverterSafeUtils
)
target_include_directories(test_write_worker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_write_worker COMMAND test_write_worker)

add_executable(test_kicad_graphics_generators test_kicad_graphics_generators.cpp)
target_link_libraries(test_kicad_graphics_generators PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_kicad_graphics_generators PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_kicad_graphics_generators COMMAND test_kicad_graphics_generators)

add_executable(test_svg_path_parser test_svg_path_parser.cpp)
target_link_libraries(test_svg_path_parser PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_svg_path_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_svg_path_parser COMMAND test_svg_path_parser)

add_executable(test_path_security test_path_security.cpp)
target_link_libraries(test_path_security PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_path_security PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_path_security COMMAND test_path_security)

add_executable(test_export_report_generator test_export_report_generator.cpp)
target_link_libraries(test_export_report_generator PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_report_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_report_generator COMMAND test_export_report_generator)

add_executable(test_export_progress_viewmodel test_export_progress_viewmodel.cpp)
target_link_libraries(test_export_progress_viewmodel PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_progress_viewmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_progress_viewmodel COMMAND test_export_progress_viewmodel)

add_executable(test_export_settings_viewmodel test_export_settings_viewmodel.cpp)
target_link_libraries(test_export_settings_viewmodel PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_settings_viewmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_settings_viewmodel COMMAND test_export_settings_viewmodel)

# 日志系统测试
add_executable(test_logging test_logging.cpp)
target_link_libraries(test_logging PRIVATE
    Qt6::Test
    EasyKiConverterLogging
    EasyKiConverterLib
)
target_include_directories(test_logging PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_logging COMMAND test_logging)

# CLI 相关测试
add_executable(test_command_line_parser test_command_line_parser.cpp)
target_link_libraries(test_command_line_parser PRIVATE
    Qt6::Test
    EasyKiConverterLib
    EasyKiConverter::Cli
)
target_include_directories(test_command_line_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_command_line_parser COMMAND test_command_line_parser)

# 语言管理器测试
add_executable(test_language_manager test_language_manager.cpp)
target_link_libraries(test_language_manager PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_language_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_language_manager COMMAND test_language_manager)

add_executable(test_completion_generator test_completion_generator.cpp)
target_link_libraries(test_completion_generator PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_completion_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_completion_generator COMMAND test_completion_generator)

add_executable(test_file_reader test_file_reader.cpp)
target_link_libraries(test_file_reader PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_file_reader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_file_reader COMMAND test_file_reader)

add_executable(test_bom_parser
    test_bom_parser.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_bom_parser PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_bom_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_bom_parser COMMAND test_bom_parser)

add_executable(test_cli_converters
    test_cli_converters.cpp
)
target_link_libraries(test_cli_converters PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_cli_converters PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_cli_converters COMMAND test_cli_converters)

add_executable(test_export_worker_helpers
    test_export_worker_helpers.cpp
)
target_link_libraries(test_export_worker_helpers PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_worker_helpers PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_worker_helpers COMMAND test_export_worker_helpers)

add_executable(test_test_paths
    test_test_paths.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_test_paths PRIVATE
    Qt6::Test
)
target_include_directories(test_test_paths PRIVATE
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_test_paths COMMAND test_test_paths)

add_executable(test_kicad_exporter_golden
    test_kicad_exporter_golden.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_kicad_exporter_golden PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_kicad_exporter_golden PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_kicad_exporter_golden COMMAND test_kicad_exporter_golden)

add_executable(test_xpedition_exporter test_xpedition_exporter.cpp)
target_link_libraries(test_xpedition_exporter PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_xpedition_exporter PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_xpedition_exporter COMMAND test_xpedition_exporter)

add_executable(test_kicad_library_table_manager test_kicad_library_table_manager.cpp)
target_link_libraries(test_kicad_library_table_manager PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_kicad_library_table_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_kicad_library_table_manager COMMAND test_kicad_library_table_manager)

add_executable(test_exporter_3d_model
    test_exporter_3d_model.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_exporter_3d_model PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_exporter_3d_model PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_exporter_3d_model COMMAND test_exporter_3d_model)

add_executable(test_easyeda_importer_fixtures
    test_easyeda_importer_fixtures.cpp
    ../common/TestPaths.hpp
)
target_link_libraries(test_easyeda_importer_fixtures PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)
target_include_directories(test_easyeda_importer_fixtures PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/altium
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_easyeda_importer_fixtures COMMAND test_easyeda_importer_fixtures)

# NetworkClient 接口和 MockNetworkClient 行为测试
add_executable(test_network_client
    test_network_client.cpp
    ../common/MockNetworkClient.hpp
)
target_link_libraries(test_network_client PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_network_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
# test_network_client.cpp 直接 #include <zlib.h> 构造 gzip 测试数据
if(TARGET ZLIB::ZLIB)
    target_link_libraries(test_network_client PRIVATE ZLIB::ZLIB)
elseif(ZLIB_FOUND)
    target_link_libraries(test_network_client PRIVATE ${ZLIB_LIBRARIES})
    target_include_directories(test_network_client PRIVATE ${ZLIB_INCLUDE_DIRS})
endif()
add_test(NAME test_network_client COMMAND test_network_client)

# CadDataLoader 业务层错误检查测试
add_executable(test_cad_data_loader test_cad_data_loader.cpp)
target_link_libraries(test_cad_data_loader PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_cad_data_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
add_test(NAME test_cad_data_loader COMMAND test_cad_data_loader)

# ComponentListViewModel 删除正在验证的元器件测试
add_executable(test_component_list_viewmodel test_component_list_viewmodel.cpp)
target_link_libraries(test_component_list_viewmodel PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_component_list_viewmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
add_test(NAME test_component_list_viewmodel COMMAND test_component_list_viewmodel)

add_executable(test_validation_state_manager test_validation_state_manager.cpp)
target_link_libraries(test_validation_state_manager PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_validation_state_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_validation_state_manager COMMAND test_validation_state_manager)

add_executable(test_update_checker_service
    test_update_checker_service.cpp
    ../common/MockNetworkClient.hpp
)
target_link_libraries(test_update_checker_service PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_update_checker_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests
)
add_test(NAME test_update_checker_service COMMAND test_update_checker_service)

set(EASYKICONVERTER_UNIT_TESTS
    test_easyeda_api
    test_network_client
    test_models
    test_component_cache_service
    test_cache_pruner
    test_core_utils
    test_atomic_file_writer
    test_temp_file_manager
    test_config_service
    test_component_service
    test_export_type_stage
    test_export_progress
    test_fetch_worker
    test_process_worker
    test_write_worker
    test_kicad_graphics_generators
    test_svg_path_parser
    test_path_security
    test_export_report_generator
    test_export_progress_viewmodel
    test_export_settings_viewmodel
    test_logging
    test_command_line_parser
    test_language_manager
    test_completion_generator
    test_file_reader
    test_bom_parser
    test_cli_converters
    test_export_worker_helpers
    test_test_paths
    test_kicad_exporter_golden
    test_xpedition_exporter
    test_kicad_library_table_manager
    test_exporter_3d_model
    test_easyeda_importer_fixtures
    test_cad_data_loader
    test_component_list_viewmodel
    test_validation_state_manager
    test_update_checker_service
)

set_tests_properties(${EASYKICONVERTER_UNIT_TESTS}
    PROPERTIES
        LABELS "unit;fast"
)
