cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(agent LANGUAGES CXX)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_BINARY_DIR}/core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../app ${CMAKE_BINARY_DIR}/app)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../llm ${CMAKE_BINARY_DIR}/llm)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.91 REQUIRED COMPONENTS json)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

file(GLOB SRC_SOURCES CONFIGURE_DEPENDS
    src/agent_server.cpp
    src/agent_chat_participant.cpp
    src/agent_manager.cpp
)
add_library(agent SHARED ${SRC_SOURCES})
set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)
target_compile_definitions(agent PRIVATE PROJ_ROOT="${PROJ_ROOT}")
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

target_include_directories(agent PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../llm/include
)
target_link_libraries(agent PUBLIC Boost::json core app llm access yaml-cpp::yaml-cpp)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
find_package(GTest REQUIRED)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    test/*.cpp
)
add_executable(tests ${TEST_SOURCES})
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests agent GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_custom_target(run_tests
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:tests>
    DEPENDS tests
)

# Google Benchmark is optional; skip on Linux where the Windows anaconda version is incompatible
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(benchmark REQUIRED)
    function(add_benchmark target src)
        add_executable(${target} ${src})
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
        target_link_libraries(${target} PRIVATE agent benchmark::benchmark)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
        list(APPEND BENCH_COMMANDS COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:${target}> --benchmark_min_time=0.1)
    endforeach()

    add_custom_target(run_bench
        ${BENCH_COMMANDS}
        DEPENDS ${BENCH_TARGETS}
        COMMENT "Running all benchmarks..."
    )
else()
    add_custom_target(run_bench
        COMMAND ${CMAKE_COMMAND} -E echo "Benchmarks skipped on Linux"
    )
endif()
endif()
