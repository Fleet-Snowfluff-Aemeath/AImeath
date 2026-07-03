cmake_minimum_required(VERSION 3.12)
project(gameserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(module/core)
add_subdirectory(module/app)
add_subdirectory(module/llm)
if(EXISTS ${CMAKE_SOURCE_DIR}/module/agent/CMakeLists.txt)
  add_subdirectory(module/agent)
endif()
file(GLOB APP_DIRS CONFIGURE_DEPENDS desktop/app/*)
foreach(dir ${APP_DIRS})
  if(EXISTS ${dir}/backend/CMakeLists.txt)
    add_subdirectory(${dir}/backend)
  endif()
endforeach()

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_executable(gameserver main.cpp)
target_link_libraries(gameserver app core Threads::Threads dl)
add_dependencies(gameserver snake gomoku pacman go)

set_target_properties(gameserver PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

target_include_directories(gameserver PRIVATE
    ${CMAKE_SOURCE_DIR}/module/app/include
)

add_executable(chat_tests desktop/app/chat/backend/test/chat_test.cpp)
target_link_libraries(chat_tests chat GTest::GTest GTest::Main)
target_include_directories(chat_tests PRIVATE desktop/app/chat/backend/include)
set_target_properties(chat_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(chat_tests chat)

find_package(benchmark REQUIRED)
add_executable(chat_bench desktop/app/chat/backend/benchmark/chat_bench.cpp)
target_link_libraries(chat_bench chat benchmark::benchmark)
target_include_directories(chat_bench PRIVATE desktop/app/chat/backend/include)
set_target_properties(chat_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(chat_bench chat)

add_executable(filemanager_tests desktop/app/filemanager/backend/test/filemanager_test.cpp)
target_link_libraries(filemanager_tests filemanager GTest::GTest GTest::Main)
target_include_directories(filemanager_tests PRIVATE desktop/app/filemanager/backend/include module/core/include)
set_target_properties(filemanager_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(filemanager_tests filemanager)

add_executable(filemanager_bench desktop/app/filemanager/backend/benchmark/filemanager_bench.cpp)
target_link_libraries(filemanager_bench filemanager benchmark::benchmark)
target_include_directories(filemanager_bench PRIVATE desktop/app/filemanager/backend/include module/core/include)
set_target_properties(filemanager_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(filemanager_bench filemanager)

add_executable(go_tests desktop/app/go/backend/test/go_test.cpp)
target_link_libraries(go_tests go GTest::GTest GTest::Main)
target_include_directories(go_tests PRIVATE
    desktop/app/go/backend/include
    desktop/app/shared/game/include
    module/app/include
)
set_target_properties(go_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(go_tests go)

add_executable(go_bench desktop/app/go/backend/benchmark/go_bench.cpp)
target_link_libraries(go_bench go benchmark::benchmark)
target_include_directories(go_bench PRIVATE
    desktop/app/go/backend/include
    desktop/app/shared/game/include
)
set_target_properties(go_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(go_bench go)

add_executable(gomoku_tests desktop/app/gomoku/backend/test/gomoku_test.cpp)
target_link_libraries(gomoku_tests gomoku GTest::GTest GTest::Main)
target_include_directories(gomoku_tests PRIVATE
    desktop/app/gomoku/backend/include
    desktop/app/shared/game/include
    module/app/include
)
set_target_properties(gomoku_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(gomoku_tests gomoku)

add_executable(gomoku_bench desktop/app/gomoku/backend/benchmark/gomoku_bench.cpp)
target_link_libraries(gomoku_bench gomoku benchmark::benchmark)
target_include_directories(gomoku_bench PRIVATE
    desktop/app/gomoku/backend/include
    desktop/app/shared/game/include
)
set_target_properties(gomoku_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(gomoku_bench gomoku)

add_executable(pacman_tests desktop/app/pacman/backend/test/pacman_test.cpp)
target_link_libraries(pacman_tests pacman GTest::GTest GTest::Main)
target_include_directories(pacman_tests PRIVATE /usr/local/boost_1_91/include)
target_include_directories(pacman_tests PRIVATE
    desktop/app/pacman/backend/include
    desktop/app/shared/game/include
    module/app/include
)
set_target_properties(pacman_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(pacman_tests pacman)

add_executable(pacman_bench desktop/app/pacman/backend/benchmark/pacman_bench.cpp)
target_link_libraries(pacman_bench pacman benchmark::benchmark)
target_include_directories(pacman_bench PRIVATE /usr/local/boost_1_91/include module/app/include)
target_include_directories(pacman_bench PRIVATE
    desktop/app/pacman/backend/include
    desktop/app/shared/game/include
)
set_target_properties(pacman_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(pacman_bench pacman)

add_executable(snake_tests desktop/app/snake/backend/test/snake_test.cpp)
target_link_libraries(snake_tests snake GTest::GTest GTest::Main)
target_include_directories(snake_tests PRIVATE
    desktop/app/snake/backend/include
    desktop/app/shared/game/include
    module/app/include
    /usr/local/boost_1_91/include
)
set_target_properties(snake_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(snake_tests snake)

add_executable(snake_bench desktop/app/snake/backend/benchmark/snake_bench.cpp)
target_link_libraries(snake_bench snake benchmark::benchmark)
target_include_directories(snake_bench PRIVATE
    desktop/app/snake/backend/include
    desktop/app/shared/game/include
    module/app/include
    /usr/local/boost_1_91/include
)
set_target_properties(snake_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(snake_bench snake)

add_executable(terminal_tests desktop/app/terminal/backend/test/terminal_test.cpp)
target_link_libraries(terminal_tests terminal GTest::GTest GTest::Main)
target_include_directories(terminal_tests PRIVATE
    desktop/app/terminal/backend/include
    module/core/include
    /usr/local/boost_1_91/include
)
set_target_properties(terminal_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(terminal_tests terminal)

add_executable(terminal_bench desktop/app/terminal/backend/benchmark/terminal_bench.cpp)
target_link_libraries(terminal_bench terminal benchmark::benchmark)
target_include_directories(terminal_bench PRIVATE
    desktop/app/terminal/backend/include
    module/core/include
    /usr/local/boost_1_91/include
)
set_target_properties(terminal_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(terminal_bench terminal)

add_executable(game_shared_tests
    desktop/app/shared/game/test/game_base_test.cpp
    desktop/app/shared/game/test/direction_test.cpp
)
target_link_libraries(game_shared_tests GTest::GTest GTest::Main)
target_include_directories(game_shared_tests PRIVATE
    desktop/app/shared/game/include
    module/app/include
    /usr/local/boost_1_91/include
)
target_link_libraries(game_shared_tests /usr/local/boost_1_91/lib/libboost_json.so)
set_target_properties(game_shared_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)

add_executable(game_shared_bench desktop/app/shared/game/benchmark/game_bench.cpp)
target_link_libraries(game_shared_bench benchmark::benchmark)
target_include_directories(game_shared_bench PRIVATE desktop/app/shared/game/include)
set_target_properties(game_shared_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)

add_executable(direction_bench desktop/app/shared/game/benchmark/direction_bench.cpp)
target_link_libraries(direction_bench benchmark::benchmark)
target_include_directories(direction_bench PRIVATE desktop/app/shared/game/include)
set_target_properties(direction_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)

add_executable(app_tests module/app/test/app_test.cpp module/app/test/app_manager_test.cpp)
target_link_libraries(app_tests app GTest::GTest GTest::Main)
target_include_directories(app_tests PRIVATE module/app/include module/core/include)
set_target_properties(app_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)
add_dependencies(app_tests mock_app)

add_executable(app_bench module/app/benchmark/app_bench.cpp)
target_link_libraries(app_bench app benchmark::benchmark)
target_include_directories(app_bench PRIVATE module/app/include module/core/include)
set_target_properties(app_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
add_dependencies(app_bench mock_app)

add_executable(agent_tests module/agent/test/agent_test.cpp)
target_include_directories(agent_tests PRIVATE module/agent/include)
target_link_libraries(agent_tests agent GTest::GTest GTest::Main)
set_target_properties(agent_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)

add_executable(agent_bench module/agent/benchmark/agent_bench.cpp)
target_include_directories(agent_bench PRIVATE module/agent/include)
target_link_libraries(agent_bench agent benchmark::benchmark)
set_target_properties(agent_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)

add_executable(llm_tests module/llm/test/llm_test.cpp)
target_include_directories(llm_tests PRIVATE module/llm/include)
target_link_libraries(llm_tests llm GTest::GTest GTest::Main)
set_target_properties(llm_tests PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/test
)

add_executable(llm_bench module/llm/benchmark/llm_bench.cpp)
target_include_directories(llm_bench PRIVATE module/llm/include)
target_link_libraries(llm_bench llm benchmark::benchmark)
set_target_properties(llm_bench PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/output/lib"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bench
)
