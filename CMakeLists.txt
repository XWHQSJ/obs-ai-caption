cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES CXX)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_TESTS "Build unit tests (offline, no OBS required)" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

# Core sources (no OBS / no sherpa-onnx linkage) — reused for tests too.
set(_CORE_SOURCES
  src/audio-ring-buffer.cpp
  src/audio-analyzer.cpp
  src/audio-delay-buffer.cpp
  src/mute-word-list.cpp
  src/model-finder.cpp
  src/subtitle-manager.cpp
)

# Plugin-only sources (require OBS + sherpa-onnx).
set(_PLUGIN_SOURCES
  src/asr-engine.cpp
  src/caption-filter.cpp
  src/model-downloader.cpp
  src/plugin-main.cpp
)

# Generated plugin-support for uniform obs_log() + PLUGIN_NAME / PLUGIN_VERSION.
configure_file(src/plugin-support.c.in ${CMAKE_BINARY_DIR}/generated/plugin-support.c @ONLY)
list(APPEND _PLUGIN_SOURCES ${CMAKE_BINARY_DIR}/generated/plugin-support.c)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# ---- sherpa-onnx (downloaded via buildspec) ----
# `${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx-${OS_LABEL}/` contains include/ and lib/.
find_library(SHERPA_ONNX_LIB sherpa-onnx-c-api
  PATHS
    "${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx/lib"
    "${CMAKE_SOURCE_DIR}/deps/sherpa-onnx/lib"
  NO_DEFAULT_PATH
)
find_path(SHERPA_ONNX_INCLUDE sherpa-onnx/c-api/c-api.h
  PATHS
    "${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx/include"
    "${CMAKE_SOURCE_DIR}/deps/sherpa-onnx/include"
  NO_DEFAULT_PATH
)
if(NOT SHERPA_ONNX_LIB OR NOT SHERPA_ONNX_INCLUDE)
  message(FATAL_ERROR
    "sherpa-onnx not found. Run buildspec fetch or place prebuilt libs under "
    ".deps/sherpa-onnx/{include,lib}/ for your platform.")
endif()
add_library(SherpaOnnx::CApi SHARED IMPORTED)
if(WIN32)
  set_target_properties(SherpaOnnx::CApi PROPERTIES
    IMPORTED_IMPLIB "${SHERPA_ONNX_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SHERPA_ONNX_INCLUDE}"
  )
else()
  set_target_properties(SherpaOnnx::CApi PROPERTIES
    IMPORTED_LOCATION "${SHERPA_ONNX_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SHERPA_ONNX_INCLUDE}"
  )
endif()
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE SherpaOnnx::CApi)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${_CORE_SOURCES} ${_PLUGIN_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# ---- Bundle sherpa-onnx runtime libraries with the plugin ----
set(_SHERPA_LIB_DIR "${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx/lib")
if(NOT EXISTS "${_SHERPA_LIB_DIR}")
  set(_SHERPA_LIB_DIR "${CMAKE_SOURCE_DIR}/deps/sherpa-onnx/lib")
endif()

if(APPLE)
  file(GLOB _sherpa_dylibs "${_SHERPA_LIB_DIR}/*.dylib")
  foreach(_dylib ${_sherpa_dylibs})
    get_filename_component(_name_only "${_dylib}" NAME)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Frameworks"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dylib}"
        "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Frameworks/${_name_only}"
      COMMAND install_name_tool -id "@rpath/${_name_only}"
        "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Frameworks/${_name_only}"
        || true
      VERBATIM
    )
  endforeach()
  set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY
    BUILD_RPATH "@loader_path/../Frameworks")
elseif(WIN32)
  # Windows: copy DLLs next to the plugin's .dll so they resolve via PATH.
  file(GLOB _sherpa_dlls "${_SHERPA_LIB_DIR}/*.dll")
  foreach(_dll ${_sherpa_dlls})
    get_filename_component(_name_only "${_dll}" NAME)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_dll}" "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${_name_only}"
      VERBATIM
    )
  endforeach()
endif()

# ---- Tests (offline, no OBS/sherpa-onnx) ----
if(ENABLE_TESTS)
  enable_testing()
  add_executable(obs-ai-caption-tests
    ${_CORE_SOURCES}
    tests/test-main.cpp
    tests/subtitle-manager-test.cpp
    tests/mute-word-list-test.cpp
    tests/audio-ring-buffer-test.cpp
    tests/audio-analyzer-test.cpp
    tests/audio-delay-buffer-test.cpp
    tests/model-finder-test.cpp
  )
  target_include_directories(obs-ai-caption-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
  if(NOT WIN32)
    target_link_libraries(obs-ai-caption-tests PRIVATE pthread)
  endif()
  add_test(NAME obs-ai-caption-tests COMMAND obs-ai-caption-tests)
endif()
