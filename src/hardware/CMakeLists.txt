add_library(Hardware
    ILiquidHandlerDriver.h
    EvoDriver.h
    EvoDriver.cpp
    FluentDriver.h
    FluentDriver.cpp
)
target_include_directories(Hardware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Hardware PUBLIC Qt${QT_VERSION_MAJOR}::Core)
