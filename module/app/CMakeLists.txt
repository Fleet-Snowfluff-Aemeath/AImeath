cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(app LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory (clear to search system paths)")
if(BOOST_ROOT AND NOT EXISTS "${BOOST_ROOT}")
    set(BOOST_ROOT "" CACHE PATH "Boost installation root directory (clear to search system paths)" FORCE)
endif()
find_package(Boost REQUIRED COMPONENTS json filesystem)

add_library(app SHARED
    src/app_mod.cpp
)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(app PRIVATE Boost::json Boost::filesystem dl)

# ---- Mock .so for testing ----
add_library(mock_app MODULE
    test/mock_app.cpp
)
target_include_directories(mock_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(mock_app PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/app_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests app GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_dependencies(tests mock_app)
add_custom_target(run_tests
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:tests>
    DEPENDS tests
)

find_package(benchmark REQUIRED)
add_executable(bench_app benchmark/app_bench.cpp)
set_target_properties(bench_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_app app benchmark::benchmark)
target_include_directories(bench_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_dependencies(bench_app mock_app)
add_custom_target(run_bench_app
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:bench_app> --benchmark_min_time=0.1
    DEPENDS bench_app
)
endif()
