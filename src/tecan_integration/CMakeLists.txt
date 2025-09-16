# Tecan integration module

# Create tecan integration library
add_library(tecan_integration STATIC
    tecanwindow.cpp
    tecanwindow.h
    gwlgenerator.cpp
    gwlgenerator.h
    generategwldialog.cpp
    generategwldialog.h
    standardlibrary.cpp
    standardlibrary.h
    standardselectiondialog.cpp
    standardselectiondialog.h
    tecanwindow.ui
    standardselectiondialog.ui
    ../../resources.qrc
)

# Link dependencies
target_link_libraries(tecan_integration
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
    PRIVATE
        plate_management
        common
)

# Include directories
target_include_directories(tecan_integration
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)
