cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(chat LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory")
find_package(Boost REQUIRED COMPONENTS json)

add_library(chat SHARED
    src/chat.cpp
    src/chat_server.cpp
)
target_include_directories(chat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(chat PRIVATE Boost::json)

if(TARGET app)
    target_include_directories(chat PRIVATE ${CMAKE_SOURCE_DIR}/module/app/include)
else()
    target_include_directories(chat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/app/include)
endif()

if(TARGET core)
    target_link_libraries(chat PRIVATE core)
    target_include_directories(chat PRIVATE ${CMAKE_SOURCE_DIR}/module/core/include)
else()
    target_include_directories(chat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../module/core/include)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/chat_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests chat GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(benchmark REQUIRED)
add_executable(bench_chat benchmark/chat_bench.cpp)
set_target_properties(bench_chat PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_chat chat benchmark::benchmark)
target_include_directories(bench_chat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
