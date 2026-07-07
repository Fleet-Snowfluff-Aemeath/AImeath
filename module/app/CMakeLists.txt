cmake_minimum_required(VERSION 3.12)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(app LANGUAGES CXX)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_BINARY_DIR}/core)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.91 REQUIRED COMPONENTS json filesystem)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

file(GLOB SRC_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)
add_library(app SHARED ${SRC_SOURCES})
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include)
target_link_libraries(app PUBLIC Boost::json Boost::filesystem dl core)

# 功能测试 && 性能测试
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
# ---- Mock .so for testing ----
add_library(mock_app MODULE
    test/mock/mock_app.cpp
)
target_include_directories(mock_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include)

find_package(GTest REQUIRED)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    test/*.cpp
)
add_executable(tests ${TEST_SOURCES})
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test)
target_link_libraries(tests app GTest::GTest GTest::Main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include)
add_dependencies(tests mock_app)
add_custom_target(run_tests
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:tests>
    DEPENDS tests
)

find_package(benchmark REQUIRED)
# ===================== 1. 定义基准测试通用函数 =====================
function(add_benchmark target src)
    add_executable(${target} ${src})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench)
    target_link_libraries(${target} PRIVATE app core benchmark::benchmark)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endfunction()

# ===================== 2. 自动扫描 + 批量创建测试程序 =====================
file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS benchmark/*.cpp)
set(BENCH_TARGETS "")

# 循环创建所有bench目标
foreach(bench_src ${BENCH_SOURCES})
    get_filename_component(bench_name ${bench_src} NAME_WE)
    set(target_name "${bench_name}")
    add_benchmark(${target_name} ${bench_src})
    list(APPEND BENCH_TARGETS ${target_name})
endforeach()
# ===================== 3. 单独处理特殊依赖 ================================

# ===================== 4. 自动拼接运行命令（修复语法错误） =====================
set(BENCH_COMMANDS "")
foreach(target ${BENCH_TARGETS})
    list(APPEND BENCH_COMMANDS COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/output/lib $<TARGET_FILE:${target}> --benchmark_min_time=0.1)
endforeach()

# 自定义目标：一键编译+运行所有基准测试
add_custom_target(run_bench
    ${BENCH_COMMANDS}
    DEPENDS ${BENCH_TARGETS} mock_app
    COMMENT "Running all benchmarks..."
)
endif()
