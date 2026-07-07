cmake_minimum_required(VERSION 3.12)
project(AImeath LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.8.0
    EXCLUDE_FROM_ALL
    SYSTEM
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(yaml-cpp)

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
target_link_libraries(AImeath app core Threads::Threads dl)

set_target_properties(AImeath PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

target_include_directories(AImeath PRIVATE
    ${CMAKE_SOURCE_DIR}/module/app/include
)
