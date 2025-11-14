# UI module

# Create UI library
add_library(ui STATIC
    databaseviewwindow.cpp
    databaseviewwindow.h
    databaseviewwindow.ui
    additemdialog.cpp
    additemdialog.h
    additemdialog.ui
    loadexperimentdialog.cpp
    loadexperimentdialog.h
    loadexperimentdialog.ui
)

# Link dependencies
target_link_libraries(ui
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Network
    PRIVATE
        database
        common
        authentication
        tecan_integration
        plate_management
        update_management
)

# Include directories
target_include_directories(ui
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)
