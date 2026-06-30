cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(filemanager LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory (clear to search system paths)")
if(BOOST_ROOT AND NOT EXISTS "${BOOST_ROOT}")
    set(BOOST_ROOT "" CACHE PATH "Boost installation root directory (clear to search system paths)" FORCE)
endif()
find_package(Boost REQUIRED COMPONENTS json)

add_library(filemanager SHARED
    src/filemgr.cpp
)
target_include_directories(filemanager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(filemanager PRIVATE ${CMAKE_SOURCE_DIR}/module/app/include)
target_link_libraries(filemanager PRIVATE Boost::json)

set(CORE_DEP_LIBS)
set(CORE_DEP_DIRS)
if(TARGET core)
    target_link_libraries(filemanager PRIVATE core)
    target_include_directories(filemanager PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
else()
    target_include_directories(filemanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/core/include)
    set(CORE_ABS ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/core/build/output/lib)
    set(CORE_DEP_LIBS ${CORE_DEP_LIBS} ${CORE_ABS}/libcore.so)
    set(CORE_DEP_DIRS ${CORE_DEP_DIRS} ${CORE_ABS})
    target_link_libraries(filemanager PRIVATE ${CORE_ABS}/libcore.so)
endif()
