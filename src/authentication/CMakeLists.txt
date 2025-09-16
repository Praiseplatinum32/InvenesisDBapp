# Authentication module

# Set source files
set(AUTHENTICATION_SOURCES
    logindialog.cpp
    resetpassworddialog.cpp
    qtbcrypt.cpp
)

set(AUTHENTICATION_HEADERS
    logindialog.h
    resetpassworddialog.h
    qtbcrypt.h
)

set(AUTHENTICATION_FORMS
    logindialog.ui
    resetpassworddialog.ui
)

# Create authentication library
add_library(authentication STATIC
    ${AUTHENTICATION_SOURCES}
    ${AUTHENTICATION_HEADERS}
    ${AUTHENTICATION_FORMS}
)

# Link dependencies
target_link_libraries(authentication
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
    PRIVATE
        common
        openwall_crypt
)

# Include directories
target_include_directories(authentication
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)