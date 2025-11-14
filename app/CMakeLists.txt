# Main application module

# Create the main application executable
add_executable(Invenesisapp
    main.cpp
    ../resources.qrc
)


# Link all required modules
target_link_libraries(Invenesisapp
    PRIVATE
        ui
        authentication
        database
        plate_management
        tecan_integration
        common
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
)

# Include directories
target_include_directories(Invenesisapp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Set target properties
set_target_properties(Invenesisapp PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

#set app version
set(APP_VERSION "1.0.0")
add_definitions(-DAPP_VERSION=\"${APP_VERSION}\")
set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")
target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
