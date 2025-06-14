cmake_minimum_required(VERSION 3.16)

project(Invenesisapp VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/openwall_crypt)

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
    openwall_crypt/crypt_blowfish.c
    openwall_crypt/crypt_gensalt.c
    openwall_crypt/wrapper.c
    PlateMapDialog.cpp
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

)

set(FORMS
    resetpassworddialog.ui
    logindialog.ui
    databaseviewwindow.ui
    tecanwindow.ui
    additemdialog.ui
    PlateMapDialog.ui
)

set(RESOURCES
    resources.qrc  # clearly add this line
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Invenesisapp
        MANUAL_FINALIZATION
        ${SOURCES}
        ${HEADERS}
        ${FORMS}
        ${RESOURCES}
        loadexperimentdialog.h loadexperimentdialog.cpp loadexperimentdialog.ui
        gwlgenerator.h gwlgenerator.cpp
        standardselectiondialog.h standardselectiondialog.cpp standardselectiondialog.ui
        standardlibrary.h standardlibrary.cpp
        .gitignore
        PlateWidget.h PlateWidget.cpp
        Plate1536Dialog.h Plate1536Dialog.cpp
)
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Invenesisapp APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Invenesisapp SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Invenesisapp
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Invenesisapp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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

include(GNUInstallDirs)
install(TARGETS Invenesisapp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Invenesisapp)
endif()
