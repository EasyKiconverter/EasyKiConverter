find_package(Qt6 REQUIRED COMPONENTS Gui Test)

add_executable(test_fixture_to_kicad_export
    test_fixture_to_kicad_export.cpp
    ../common/TestPaths.hpp
)

add_executable(test_parallel_export_cancellation
    test_parallel_export_cancellation.cpp
    ../common/TestPaths.hpp
)

target_link_libraries(test_fixture_to_kicad_export PRIVATE
    Qt6::Test
    Qt6::Gui
    EasyKiConverterLib
)

target_link_libraries(test_parallel_export_cancellation PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)

target_include_directories(test_fixture_to_kicad_export PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/altium
    ${CMAKE_SOURCE_DIR}
)

target_include_directories(test_parallel_export_cancellation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME test_fixture_to_kicad_export COMMAND test_fixture_to_kicad_export)
add_test(NAME test_parallel_export_cancellation COMMAND test_parallel_export_cancellation)

set_tests_properties(test_fixture_to_kicad_export
    PROPERTIES
        LABELS "integration;export-flow"
)

set_tests_properties(test_parallel_export_cancellation
    PROPERTIES
        LABELS "integration;export-flow"
)
