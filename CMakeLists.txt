cmake_minimum_required(VERSION 3.12)
project(AImeath LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
    EXCLUDE_FROM_ALL
    SYSTEM
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(yaml-cpp)

find_package(Boost 1.91 QUIET COMPONENTS json url filesystem)
if(Boost_FOUND)
    message(STATUS "Using system Boost ${Boost_VERSION}")
else()
    message(STATUS "Boost not found, fetching from GitHub")
    FetchContent_Declare(Boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.91.0-1
        SYSTEM
        OVERRIDE_FIND_PACKAGE
    )
    set(BOOST_INCLUDE_LIBRARIES json url filesystem)
    FetchContent_MakeAvailable(Boost)
endif()

file(GLOB MODULE_DIRS CONFIGURE_DEPENDS module/*)
foreach(module_dir ${MODULE_DIRS})
    add_subdirectory(${module_dir})
endforeach()

file(GLOB APP_DIRS CONFIGURE_DEPENDS desktop/app/*/backend)
foreach(app_dir ${APP_DIRS})
    add_subdirectory(${app_dir})
endforeach()

find_package(Threads REQUIRED)

add_executable(AImeath main.cpp)
target_link_libraries(AImeath app core access Threads::Threads dl)

set_target_properties(AImeath PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

target_include_directories(AImeath PRIVATE
    ${CMAKE_SOURCE_DIR}/module/app/include
)
