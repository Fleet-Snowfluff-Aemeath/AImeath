cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Core LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "/usr/local/boost_1_91" CACHE PATH "Boost installation root directory (clear to search system paths)")
if(BOOST_ROOT AND NOT EXISTS "${BOOST_ROOT}")
    set(BOOST_ROOT "" CACHE PATH "Boost installation root directory (clear to search system paths)" FORCE)
endif()
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS url json filesystem)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../app ${CMAKE_BINARY_DIR}/app)
endif()

add_library(core SHARED
    src/threadmgr.cpp
    src/timer.cpp
    src/logger.cpp
    src/config.cpp
    src/eventmgr.cpp
    src/netconn.cpp
    src/wsutil.cpp
    src/ws_server.cpp
    src/vfs.cpp
    src/pty_session.cpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads Boost::url Boost::json util)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
add_executable(tests
    test/threadpool_test.cpp
    test/timer_test.cpp
    test/logger_test.cpp
    test/eventmgr_test.cpp
    test/netconn_test.cpp
    test/ws_server_test.cpp
    test/message_queue_test.cpp
    test/vfs_test.cpp
    test/pty_session_test.cpp
)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests core app GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(benchmark REQUIRED)

add_executable(bench_threadpool benchmark/threadpool_bench.cpp)
set_target_properties(bench_threadpool PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_threadpool core benchmark::benchmark)
target_include_directories(bench_threadpool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench_timer benchmark/timer_bench.cpp)
set_target_properties(bench_timer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_timer core benchmark::benchmark)
target_include_directories(bench_timer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench_logger benchmark/logger_bench.cpp)
set_target_properties(bench_logger PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_logger core benchmark::benchmark)
target_include_directories(bench_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench_eventmgr benchmark/eventmgr_bench.cpp)
set_target_properties(bench_eventmgr PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_eventmgr core benchmark::benchmark)
target_include_directories(bench_eventmgr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench_netconn benchmark/netconn_bench.cpp)
set_target_properties(bench_netconn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_netconn core benchmark::benchmark)
target_include_directories(bench_netconn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench_ws_server benchmark/ws_server_bench.cpp)
set_target_properties(bench_ws_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
target_link_libraries(bench_ws_server core app benchmark::benchmark)
target_include_directories(bench_ws_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(bench_ws_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../app/include)

add_custom_target(bench
    COMMAND $<TARGET_FILE:bench_threadpool>
    COMMAND $<TARGET_FILE:bench_timer>
    COMMAND $<TARGET_FILE:bench_logger>
    COMMAND $<TARGET_FILE:bench_eventmgr>
    COMMAND $<TARGET_FILE:bench_netconn>
    COMMAND $<TARGET_FILE:bench_ws_server>
    DEPENDS bench_threadpool bench_timer bench_logger bench_eventmgr bench_netconn bench_ws_server
)
endif()
