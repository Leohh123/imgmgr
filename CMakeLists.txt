cmake_minimum_required(VERSION 3.20)

project(imgmgr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Concurrent Test)

enable_testing()

function(configure_imgmgr_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endfunction()

qt_add_executable(imgmgr
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/ImageRepository.h
    src/database/ImageRepository.cpp
    src/database/RuleRepository.h
    src/database/RuleRepository.cpp
    src/database/SqlUtils.h
    src/database/SqlUtils.cpp
    src/models/ImageListModel.h
    src/models/ImageListModel.cpp
    src/models/RuleTreeModel.h
    src/models/RuleTreeModel.cpp
    src/services/ProjectScanner.h
    src/services/ProjectScanner.cpp
    src/services/ProjectSession.h
    src/services/ProjectSession.cpp
    src/services/ThumbnailCache.h
    src/services/ThumbnailCache.cpp
    src/services/RuleEngine.h
    src/services/RuleEngine.cpp
    src/services/ProjectStatsService.h
    src/services/ProjectStatsService.cpp
    src/services/ProjectPathService.h
    src/services/ProjectPathService.cpp
    src/services/RuleJsonService.h
    src/services/RuleJsonService.cpp
    src/services/RuleValidationService.h
    src/services/RuleValidationService.cpp
    src/widgets/ImagePreviewWidget.h
    src/widgets/ImagePreviewWidget.cpp
    src/widgets/FilterPanel.h
    src/widgets/FilterPanel.cpp
    src/widgets/RulePanel.h
    src/widgets/RulePanel.cpp
    src/widgets/RuleEditDialog.h
    src/widgets/RuleEditDialog.cpp
    src/utils/FileIoUtils.h
    src/utils/FileIoUtils.cpp
    src/utils/HashUtils.h
    src/utils/HashUtils.cpp
    src/utils/ImageUtils.h
    src/utils/ImageUtils.cpp
    src/utils/PaintUtils.h
    src/utils/PaintUtils.cpp
    src/utils/ProjectFileDialogs.h
    src/utils/ProjectFileDialogs.cpp
    src/utils/RecentProjectsStore.h
    src/utils/RecentProjectsStore.cpp
    src/utils/RuleExplanationBuilder.h
    src/utils/RuleExplanationBuilder.cpp
    src/utils/RuleGraphUtils.h
    src/utils/RuleGraphUtils.cpp
    src/utils/RuleUtils.h
    src/utils/RuleUtils.cpp
    src/utils/UiUtils.h
    src/utils/UiUtils.cpp
    src/utils/ImageTableUtils.h
    src/utils/ImageTableUtils.cpp
    src/types.h
)

target_include_directories(imgmgr PRIVATE src)
target_link_libraries(imgmgr PRIVATE Qt6::Widgets Qt6::Sql Qt6::Concurrent)
configure_imgmgr_warnings(imgmgr)

function(configure_imgmgr_test target)
    target_include_directories(${target} PRIVATE src tests)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Test)
    configure_imgmgr_warnings(${target})
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE FALSE)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt6::Test> $<TARGET_FILE_DIR:${target}>)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_library(imgmgr_test_support STATIC
    tests/TestDbUtils.h
    tests/TestDbUtils.cpp
    src/types.h
)
target_include_directories(imgmgr_test_support PUBLIC src tests)
target_link_libraries(imgmgr_test_support PUBLIC Qt6::Core)
configure_imgmgr_warnings(imgmgr_test_support)

set(IMGMGR_RULE_CORE_TEST_SOURCES
    src/utils/RuleGraphUtils.h
    src/utils/RuleGraphUtils.cpp
    src/utils/RuleUtils.h
    src/utils/RuleUtils.cpp
    src/types.h
)

set(IMGMGR_DATABASE_CORE_TEST_SOURCES
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/ImageRepository.h
    src/database/ImageRepository.cpp
    src/database/RuleRepository.h
    src/database/RuleRepository.cpp
    src/database/SqlUtils.h
    src/database/SqlUtils.cpp
    ${IMGMGR_RULE_CORE_TEST_SOURCES}
)

set(IMGMGR_RULE_ENGINE_CORE_TEST_SOURCES
    src/services/RuleEngine.h
    src/services/RuleEngine.cpp
    ${IMGMGR_DATABASE_CORE_TEST_SOURCES}
)

qt_add_executable(imgmgr_rule_tests
    tests/RuleUtilsTests.cpp
    src/services/RuleJsonService.h
    src/services/RuleJsonService.cpp
    src/services/RuleValidationService.h
    src/services/RuleValidationService.cpp
    src/utils/RuleExplanationBuilder.h
    src/utils/RuleExplanationBuilder.cpp
    ${IMGMGR_RULE_CORE_TEST_SOURCES}
)
configure_imgmgr_test(imgmgr_rule_tests)

qt_add_executable(imgmgr_path_tests
    tests/ProjectPathServiceTests.cpp
    src/services/ProjectPathService.h
    src/services/ProjectPathService.cpp
)
configure_imgmgr_test(imgmgr_path_tests)

qt_add_executable(imgmgr_file_io_tests
    tests/FileIoUtilsTests.cpp
    src/utils/FileIoUtils.h
    src/utils/FileIoUtils.cpp
)
configure_imgmgr_test(imgmgr_file_io_tests)

qt_add_executable(imgmgr_hash_tests
    tests/HashUtilsTests.cpp
    src/utils/HashUtils.h
    src/utils/HashUtils.cpp
)
configure_imgmgr_test(imgmgr_hash_tests)

qt_add_executable(imgmgr_image_tests
    tests/ImageUtilsTests.cpp
    src/utils/ImageUtils.h
    src/utils/ImageUtils.cpp
)
configure_imgmgr_test(imgmgr_image_tests)
target_link_libraries(imgmgr_image_tests PRIVATE Qt6::Gui)

qt_add_executable(imgmgr_filter_panel_tests
    tests/FilterPanelTests.cpp
    src/widgets/FilterPanel.h
    src/widgets/FilterPanel.cpp
    src/utils/UiUtils.h
    src/utils/UiUtils.cpp
    src/utils/RuleUtils.h
    src/utils/RuleUtils.cpp
    src/types.h
)
configure_imgmgr_test(imgmgr_filter_panel_tests)
target_link_libraries(imgmgr_filter_panel_tests PRIVATE Qt6::Widgets)

qt_add_executable(imgmgr_sql_tests
    tests/SqlUtilsTests.cpp
    src/database/SqlUtils.h
    src/database/SqlUtils.cpp
)
configure_imgmgr_test(imgmgr_sql_tests)
target_link_libraries(imgmgr_sql_tests PRIVATE Qt6::Sql imgmgr_test_support)

qt_add_executable(imgmgr_database_tests
    tests/DatabaseRepositoryTests.cpp
    ${IMGMGR_DATABASE_CORE_TEST_SOURCES}
)
configure_imgmgr_test(imgmgr_database_tests)
target_link_libraries(imgmgr_database_tests PRIVATE Qt6::Sql imgmgr_test_support)

qt_add_executable(imgmgr_rule_engine_tests
    tests/RuleEngineTests.cpp
    ${IMGMGR_RULE_ENGINE_CORE_TEST_SOURCES}
)
configure_imgmgr_test(imgmgr_rule_engine_tests)
target_link_libraries(imgmgr_rule_engine_tests PRIVATE Qt6::Sql imgmgr_test_support)

qt_add_executable(imgmgr_project_stats_tests
    tests/ProjectStatsServiceTests.cpp
    src/services/ProjectStatsService.h
    src/services/ProjectStatsService.cpp
    ${IMGMGR_RULE_ENGINE_CORE_TEST_SOURCES}
)
configure_imgmgr_test(imgmgr_project_stats_tests)
target_link_libraries(imgmgr_project_stats_tests PRIVATE Qt6::Sql imgmgr_test_support)

if (WIN32)
    set_target_properties(imgmgr PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
