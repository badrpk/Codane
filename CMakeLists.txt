cmake_minimum_required(VERSION 3.20)
project(codane VERSION 0.1.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(codane_lib
  src/json.cpp src/process.cpp src/journal.cpp src/graph.cpp src/agent.cpp
  src/codex_provider.cpp src/verifier.cpp src/loop.cpp src/runtime.cpp src/tui.cpp)
target_include_directories(codane_lib PUBLIC include)
target_compile_options(codane_lib PRIVATE -Wall -Wextra -Wpedantic)
add_executable(codane src/main.cpp src/entry.cpp)
set_source_files_properties(src/main.cpp PROPERTIES COMPILE_DEFINITIONS "main=codane_cli_main")
target_compile_definitions(codane PRIVATE CODANE_VERSION="${PROJECT_VERSION}")
target_link_libraries(codane PRIVATE codane_lib)
enable_testing()
add_executable(codane_tests tests/test_main.cpp)
target_compile_definitions(codane_tests PRIVATE CODANE_VERSION="${PROJECT_VERSION}")
target_link_libraries(codane_tests PRIVATE codane_lib)
add_test(NAME codane_tests COMMAND codane_tests)

target_sources(codane_lib PRIVATE
  src/chat_provider.cpp src/harness_provider.cpp
  src/provider_registry.cpp src/provider_router.cpp)
add_executable(codane_chat_tests tests/test_chat.cpp)
target_link_libraries(codane_chat_tests PRIVATE codane_lib)
add_test(NAME codane_chat_tests COMMAND codane_chat_tests)

target_sources(codane_lib PRIVATE src/chat_config.cpp src/chat_tui.cpp src/workspace.cpp src/conversation.cpp)
foreach(suite IN ITEMS config conversation json_chat process_chat chat_tui)
  add_executable(codane_${suite}_tests tests/test_${suite}.cpp)
  target_link_libraries(codane_${suite}_tests PRIVATE codane_lib)
  add_test(NAME codane_${suite}_tests COMMAND codane_${suite}_tests)
endforeach()

add_executable(codane_http_tests tests/test_http.cpp)
target_link_libraries(codane_http_tests PRIVATE codane_lib)
add_test(NAME codane_http_tests COMMAND codane_http_tests)

target_sources(codane_lib PRIVATE src/http_transport.cpp)
