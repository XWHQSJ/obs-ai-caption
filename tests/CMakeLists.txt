cmake_minimum_required(VERSION 3.16)
project(obs-ai-caption-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Offline tests for core modules — no OBS / sherpa-onnx / Qt required.
# Usage:
#   cmake -S tests -B build-tests
#   cmake --build build-tests -j
#   ctest --test-dir build-tests --output-on-failure

set(_CORE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/audio-ring-buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/audio-analyzer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/audio-delay-buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/mute-word-list.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/model-finder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/subtitle-manager.cpp
)

enable_testing()

add_executable(obs-ai-caption-tests
  ${_CORE_SOURCES}
  test-main.cpp
  subtitle-manager-test.cpp
  mute-word-list-test.cpp
  audio-ring-buffer-test.cpp
  audio-analyzer-test.cpp
  audio-delay-buffer-test.cpp
  model-finder-test.cpp
)

target_include_directories(obs-ai-caption-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  target_compile_options(obs-ai-caption-tests PRIVATE /W4 /utf-8 /permissive-)
  target_compile_definitions(obs-ai-caption-tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(obs-ai-caption-tests PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(obs-ai-caption-tests PRIVATE pthread)
endif()

add_test(NAME obs-ai-caption-tests COMMAND obs-ai-caption-tests)
