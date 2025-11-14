cmake_minimum_required(VERSION 3.16)

project(Invenesisapp VERSION 1.0.0 LANGUAGES CXX C)

# Project-wide settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic MOC, UIC, and RCC processing globally
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt components - support both Qt5 and Qt6
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql Network)

# Set Qt version for consistent target linking across modules
if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
endif()

# Global include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add subdirectories in dependency order
# 1. External libraries first (no dependencies)
add_subdirectory(external)

# 2. Common utilities (depends only on Qt and external)
add_subdirectory(src/common)

# 3. Core modules (depend on common and external)
add_subdirectory(src/authentication)
add_subdirectory(src/database)

# 4. Feature modules (depend on core modules)
add_subdirectory(src/plate_management)

# 5. Integration modules (depend on feature modules)
add_subdirectory(src/tecan_integration)

# 6. UI module (depends on all other modules)
add_subdirectory(src/ui)

# 7. udate module
add_subdirectory(src/update_mangement)

# 7. Main application (coordinates all modules)
add_subdirectory(app)


# Set the main application as the default startup project for IDEs
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Invenesisapp)

# Configure build output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing if requested
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
endif()

# Print configuration summary
message(STATUS "=== Invenesisapp Configuration Summary ===")
message(STATUS "Project Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt Version: ${QT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "===========================================")



