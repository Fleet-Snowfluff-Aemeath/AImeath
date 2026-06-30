cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(llm LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory (clear to search system paths)")
if(BOOST_ROOT AND NOT EXISTS "${BOOST_ROOT}")
    set(BOOST_ROOT "" CACHE PATH "Boost installation root directory (clear to search system paths)" FORCE)
endif()
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

add_library(llm SHARED
    src/llm_client.cpp
)
target_include_directories(llm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(llm PUBLIC OpenSSL::SSL OpenSSL::Crypto Boost::json)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/llm_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests llm GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(benchmark REQUIRED)
add_executable(bench_llm benchmark/llm_bench.cpp)
set_target_properties(bench_llm PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_llm llm benchmark::benchmark)
target_include_directories(bench_llm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_target(bench
    COMMAND $<TARGET_FILE:bench_llm>
    DEPENDS bench_llm
)
endif()
