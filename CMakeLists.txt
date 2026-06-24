cmake_minimum_required(VERSION 3.12)
project(gameserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(module/core)
add_subdirectory(module/app)
file(GLOB APP_DIRS CONFIGURE_DEPENDS desktop/app/*)
foreach(dir ${APP_DIRS})
  if(EXISTS ${dir}/backend/CMakeLists.txt)
    add_subdirectory(${dir}/backend)
  endif()
endforeach()

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_executable(gameserver main.cpp)
target_link_libraries(gameserver app core Threads::Threads dl)
add_dependencies(gameserver snake gomoku pacman go)

set_target_properties(gameserver PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

target_include_directories(gameserver PRIVATE
    ${CMAKE_SOURCE_DIR}/module/app/include
)

add_executable(chat_tests desktop/app/chat/backend/test/chat_test.cpp)
target_link_libraries(chat_tests chat GTest::GTest GTest::Main)
target_include_directories(chat_tests PRIVATE desktop/app/chat/backend/include)
set_target_properties(chat_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(chat_tests chat)
