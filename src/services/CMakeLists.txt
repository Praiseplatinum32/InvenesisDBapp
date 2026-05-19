add_library(Services
    ExperimentManager.cpp
    ExperimentManager.h
    DilutionEngine.h
    DilutionEngine.cpp
    gwl_helpers.h
    gwl_helpers.cpp
    Logger.h
    Logger.cpp
    JsonHelpers.h
    JsonHelpers.cpp
    ExperimentJsonSerializer.h
    ExperimentJsonSerializer.cpp
)
target_include_directories(Services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Services PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets DataAccess plate_management)
