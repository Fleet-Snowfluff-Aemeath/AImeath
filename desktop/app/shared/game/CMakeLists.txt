cmake_minimum_required(VERSION 3.12)
project(game_shared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory (clear to search system paths)")
if(BOOST_ROOT AND NOT EXISTS "${BOOST_ROOT}")
    set(BOOST_ROOT "" CACHE PATH "Boost installation root directory (clear to search system paths)" FORCE)
endif()
find_package(Boost REQUIRED COMPONENTS json)

add_library(game_shared INTERFACE)
target_include_directories(game_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(GTest REQUIRED)
  add_executable(tests
      test/game_base_test.cpp
      test/direction_test.cpp
  )
  set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
  target_link_libraries(tests GTest::GTest GTest::Main)
  target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/module/app/include)

  find_package(benchmark REQUIRED)
  add_executable(bench_game benchmark/game_bench.cpp)
  add_executable(bench_direction benchmark/direction_bench.cpp)
  set_target_properties(bench_game PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
  set_target_properties(bench_direction PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
  target_link_libraries(bench_game benchmark::benchmark)
  target_link_libraries(bench_direction benchmark::benchmark)
  target_include_directories(bench_game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(bench_direction PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
