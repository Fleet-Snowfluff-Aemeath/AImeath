cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(snake LANGUAGES CXX)
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

add_library(snake SHARED
    src/snake.cpp
    src/board.cpp
    src/game.cpp
    ${CMAKE_SOURCE_DIR}/desktop/app/shared/game/src/game_app_adapter.cpp
)
target_include_directories(snake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(snake PRIVATE ${CMAKE_SOURCE_DIR}/module/app/include)
target_include_directories(snake PRIVATE ${CMAKE_SOURCE_DIR}/desktop/app/shared/game/include)
target_link_libraries(snake PRIVATE Boost::json)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/snake_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests snake GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/app/include)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../shared/game/include)

find_package(benchmark REQUIRED)
add_executable(bench_snake benchmark/snake_bench.cpp)
set_target_properties(bench_snake PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_snake snake benchmark::benchmark)
target_include_directories(bench_snake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(bench_snake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/app/include)
target_include_directories(bench_snake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../shared/game/include)
endif()
