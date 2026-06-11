cmake_minimum_required(VERSION 3.20)

project(imgmgr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Concurrent Test)

enable_testing()

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

qt_add_executable(imgmgr_rule_tests
    tests/RuleUtilsTests.cpp
    src/services/RuleJsonService.h
    src/services/RuleJsonService.cpp
    src/services/RuleValidationService.h
    src/services/RuleValidationService.cpp
    src/utils/RuleExplanationBuilder.h
    src/utils/RuleExplanationBuilder.cpp
    src/utils/RuleUtils.h
    src/utils/RuleUtils.cpp
    src/types.h
)
target_include_directories(imgmgr_rule_tests PRIVATE src)
target_link_libraries(imgmgr_rule_tests PRIVATE Qt6::Core Qt6::Test)
set_target_properties(imgmgr_rule_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_custom_command(TARGET imgmgr_rule_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt6::Test> $<TARGET_FILE_DIR:imgmgr_rule_tests>)
add_test(NAME imgmgr_rule_tests COMMAND imgmgr_rule_tests)

qt_add_executable(imgmgr_path_tests
    tests/ProjectPathServiceTests.cpp
    src/services/ProjectPathService.h
    src/services/ProjectPathService.cpp
)
target_include_directories(imgmgr_path_tests PRIVATE src)
target_link_libraries(imgmgr_path_tests PRIVATE Qt6::Core Qt6::Test)
set_target_properties(imgmgr_path_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_custom_command(TARGET imgmgr_path_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt6::Test> $<TARGET_FILE_DIR:imgmgr_path_tests>)
add_test(NAME imgmgr_path_tests COMMAND imgmgr_path_tests)

if (MSVC)
    target_compile_options(imgmgr PRIVATE /utf-8)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgmgr PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if (WIN32)
    set_target_properties(imgmgr PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
