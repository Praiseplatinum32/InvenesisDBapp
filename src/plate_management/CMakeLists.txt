# Plate management module

# Create plate management library
add_library(plate_management STATIC
    PlateWidget.cpp
    PlateWidget.h
    PlateMapDialog.cpp
    PlateMapDialog.h
    PlateMapDialog.ui
    Plate1536Dialog.cpp
    Plate1536Dialog.h
    matrixplatecontainer.cpp
    matrixplatecontainer.h
    matrixplatewidget.cpp
    matrixplatewidget.h
    daughterplatewidget.cpp
    daughterplatewidget.h
)

# Link dependencies
target_link_libraries(plate_management
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
    PRIVATE
        common
)

# Include directories
target_include_directories(plate_management
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)