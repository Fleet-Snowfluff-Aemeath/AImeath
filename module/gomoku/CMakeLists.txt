cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(gomoku LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory")
find_package(Boost REQUIRED COMPONENTS json)

add_library(gomoku SHARED
    src/board.cpp
    src/game.cpp
)
target_include_directories(gomoku PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gomoku PRIVATE Boost::json)
if(TARGET game)
    target_link_libraries(gomoku PRIVATE game)
else()
    target_include_directories(gomoku PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/gomoku_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests gomoku GTest::GTest GTest::Main)
if(TARGET game)
    target_link_libraries(tests PRIVATE game)
endif()
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)

find_package(benchmark REQUIRED)
add_executable(bench_gomoku benchmark/gomoku_bench.cpp)
set_target_properties(bench_gomoku PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_gomoku gomoku benchmark::benchmark)
target_include_directories(bench_gomoku PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
