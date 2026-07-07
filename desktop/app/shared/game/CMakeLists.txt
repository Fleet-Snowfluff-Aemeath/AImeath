cmake_minimum_required(VERSION 3.12)
set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(game_shared LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.91 REQUIRED COMPONENTS json)

add_library(game_shared INTERFACE)
target_include_directories(game_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(GTest REQUIRED)
  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
      test/*.cpp
  )
  add_executable(tests ${TEST_SOURCES})
  set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
  target_link_libraries(tests GTest::GTest GTest::Main Boost::json)
  target_include_directories(tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${PROJ_ROOT}/module/app/include
  )
  add_custom_target(run_tests
      COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:tests>
      DEPENDS tests
  )

  find_package(benchmark REQUIRED)
  function(add_benchmark target src)
      add_executable(${target} ${src})
      set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
      target_link_libraries(${target} PRIVATE benchmark::benchmark)
      target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
  endfunction()

  file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS benchmark/*.cpp)
  set(BENCH_TARGETS "")

  foreach(bench_src ${BENCH_SOURCES})
      get_filename_component(bench_name ${bench_src} NAME_WE)
      set(target_name "${bench_name}")
      add_benchmark(${target_name} ${bench_src})
      list(APPEND BENCH_TARGETS ${target_name})
  endforeach()

  set(BENCH_COMMANDS "")
  foreach(target ${BENCH_TARGETS})
      list(APPEND BENCH_COMMANDS COMMAND $<TARGET_FILE:${target}> --benchmark_min_time=0.1)
  endforeach()

  add_custom_target(run_bench
      ${BENCH_COMMANDS}
      DEPENDS ${BENCH_TARGETS}
      COMMENT "Running all benchmarks..."
  )
endif()
