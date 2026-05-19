add_library(DataAccess
    ExperimentDao.cpp
    ExperimentDao.h
    AuthenticationDao.cpp
    AuthenticationDao.h
    ItemDao.cpp
    ItemDao.h
)
target_include_directories(DataAccess PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DataAccess PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
