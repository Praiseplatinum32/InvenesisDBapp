cmake_minimum_required(VERSION 3.16)

project(Invenesisapp VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)

# Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/openwall_crypt)

# Source groups
set(SOURCES
    main.cpp
    logindialog.cpp
    databaseviewwindow.cpp
    resetpassworddialog.cpp
    draggabletableview.cpp
    daughterplatewidget.cpp
    matrixplatecontainer.cpp
    matrixplatewidget.cpp
    customproxymodel.cpp
    tecanwindow.cpp
    additemdialog.cpp
    qtbcrypt.cpp
    PlateMapDialog.cpp
    PlateWidget.cpp
    Plate1536Dialog.cpp

    # openwall crypt c sources
    openwall_crypt/crypt_blowfish.c
    openwall_crypt/crypt_gensalt.c
    openwall_crypt/wrapper.c

    # Newly listed sources you had inside the if() block before
    loadexperimentdialog.cpp
    gwlgenerator.cpp
    standardselectiondialog.cpp
    standardlibrary.cpp
)

set(HEADERS
    database.h
    logindialog.h
    databaseviewwindow.h
    ClickableLabel.h
    draggabletableview.h
    daughterplatewidget.h
    resetpassworddialog.h
    matrixplatecontainer.h
    matrixplatewidget.h
    customproxymodel.h
    tecanwindow.h
    additemdialog.h
    qtbcrypt.h
    openwall_crypt/crypt.h
    openwall_crypt/ow-crypt.h
    openwall_crypt/crypt_blowfish.h
    openwall_crypt/crypt_gensalt.h
    PlateMapDialog.h
    PlateWidget.h
    Plate1536Dialog.h

    # Newly listed headers you had inside the if() block before
    loadexperimentdialog.h
    gwlgenerator.h
    standardselectiondialog.h
    standardlibrary.h
)

set(FORMS
    resetpassworddialog.ui
    logindialog.ui
    databaseviewwindow.ui
    tecanwindow.ui
    additemdialog.ui
    PlateMapDialog.ui

    # Newly listed form you had inside the if() block before
    loadexperimentdialog.ui
    standardselectiondialog.ui
)

set(RESOURCES
    resources.qrc
)

# Single master list for the target
set(PROJECT_SOURCES
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
)

# Executable (Qt6 vs Qt5)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Invenesisapp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        generategwldialog.h generategwldialog.cpp
    )
else()
    add_executable(Invenesisapp
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(Invenesisapp PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

# Bundle / app settings
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Invenesisapp)
endif()

set_target_properties(Invenesisapp PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Install
include(GNUInstallDirs)
install(TARGETS Invenesisapp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt6 needs finalize
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Invenesisapp)
endif()
