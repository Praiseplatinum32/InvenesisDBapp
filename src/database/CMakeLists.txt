# Database module

# Define source files
set(DATABASE_SOURCES
    customproxymodel.cpp
    draggabletableview.cpp
)

set(DATABASE_HEADERS
    Database.h
    customproxymodel.h
    draggabletableview.h
)

# Create the database module library
add_library(database STATIC
    ${DATABASE_SOURCES}
    ${DATABASE_HEADERS}
)

# Link required Qt libraries and dependencies
target_link_libraries(database
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
    PRIVATE
        common
)

# Set up include directories
target_include_directories(database
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)