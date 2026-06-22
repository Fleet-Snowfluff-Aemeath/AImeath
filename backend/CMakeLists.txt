cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(gameserver LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gameserver main.cpp)
find_package(Threads REQUIRED)
target_link_libraries(gameserver game core Threads::Threads dl)

set_target_properties(gameserver PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

target_include_directories(gameserver PRIVATE ${CMAKE_SOURCE_DIR}/module/game/include ${CMAKE_SOURCE_DIR}/module/core/include)
