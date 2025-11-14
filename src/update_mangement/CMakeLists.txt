
add_library(update_management STATIC
    UpdateChecker.cpp
    UpdateChecker.h
)

# Link dependencies
target_link_libraries(update_management
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
    PRIVATE
        database
        common
        authentication
        tecan_integration
        plate_management
        ui
)

# Include directories
target_include_directories(ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)
