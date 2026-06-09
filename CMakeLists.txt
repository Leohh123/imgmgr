cmake_minimum_required(VERSION 3.20)

project(imgmgr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Concurrent)

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
    src/widgets/ImagePreviewWidget.h
    src/widgets/ImagePreviewWidget.cpp
    src/widgets/FilterPanel.h
    src/widgets/FilterPanel.cpp
    src/widgets/RulePanel.h
    src/widgets/RulePanel.cpp
    src/utils/HashUtils.h
    src/utils/HashUtils.cpp
    src/utils/ImageUtils.h
    src/utils/ImageUtils.cpp
    src/utils/PaintUtils.h
    src/utils/PaintUtils.cpp
    src/utils/RuleUtils.h
    src/utils/RuleUtils.cpp
    src/types.h
)

target_include_directories(imgmgr PRIVATE src)
target_link_libraries(imgmgr PRIVATE Qt6::Widgets Qt6::Sql Qt6::Concurrent)

if (MSVC)
    target_compile_options(imgmgr PRIVATE /utf-8)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgmgr PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if (WIN32)
    set_target_properties(imgmgr PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
