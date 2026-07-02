cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(agent LANGUAGES CXX)
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

add_library(agent SHARED
    src/agent_server.cpp
)
target_include_directories(agent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(TARGET core)
    target_link_libraries(agent PRIVATE core)
    target_include_directories(agent PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
endif()

if(TARGET app)
    target_link_libraries(agent PUBLIC app)
    target_include_directories(agent PRIVATE ${CMAKE_SOURCE_DIR}/module/app/include)
endif()

if(TARGET llm)
    target_link_libraries(agent PRIVATE llm)
    target_include_directories(agent PRIVATE ${CMAKE_SOURCE_DIR}/module/llm/include)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/agent_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests agent GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(benchmark REQUIRED)
add_executable(bench_agent benchmark/agent_bench.cpp)
set_target_properties(bench_agent PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_agent agent benchmark::benchmark)
target_include_directories(bench_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
