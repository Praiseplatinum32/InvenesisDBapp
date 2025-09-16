# Common utilities module

# Create common library
add_library(common STATIC
    ClickableLabel.h
)

# Link dependencies
target_link_libraries(common
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Include directories
target_include_directories(common
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)