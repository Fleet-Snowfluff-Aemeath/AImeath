cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(terminal LANGUAGES CXX)
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

add_library(terminal SHARED
    src/terminal.cpp
)
target_include_directories(terminal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
get_filename_component(_proj_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../../" REALPATH)
target_include_directories(terminal PRIVATE ${_proj_root}/module/app/include)
target_link_libraries(terminal PRIVATE Boost::json util)

if(TARGET core)
    target_link_libraries(terminal PRIVATE core)
    target_include_directories(terminal PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
else()
    target_include_directories(terminal PRIVATE ${_proj_root}/module/core/include)
    target_link_libraries(terminal PRIVATE ${_proj_root}/module/core/build/output/lib/libcore.so)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/terminal_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests terminal GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(EXISTS ${CMAKE_SOURCE_DIR}/module/core/include)
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
else()
    target_include_directories(tests PRIVATE ${_proj_root}/module/core/include)
endif()

find_package(benchmark REQUIRED)
add_executable(bench_terminal benchmark/terminal_bench.cpp)
set_target_properties(bench_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_terminal terminal benchmark::benchmark)
target_include_directories(bench_terminal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(EXISTS ${CMAKE_SOURCE_DIR}/module/core/include)
    target_include_directories(bench_terminal PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
else()
    target_include_directories(bench_terminal PRIVATE ${_proj_root}/module/core/include)
endif()
endif()
