# Downloads and extracts the sherpa-onnx prebuilt tarball declared in
# buildspec.json under the `sherpa-onnx` dependency, for the current OS slice.
#
# The archive is extracted into `${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx/` and the
# `include/` + `lib/` directories live directly inside it so the top-level
# CMakeLists can find SherpaOnnx::CApi there.
#
# Expected schema in buildspec.json:
#   "sherpa-onnx": {
#     "version": "1.12.39",
#     "baseUrl": "https://github.com/k2-fsa/sherpa-onnx/releases/download",
#     "assets":  { "macos": "sherpa-onnx-v1.12.39-macos-universal-shared.tar.bz2",
#                  "windows-x64": "sherpa-onnx-v1.12.39-win-x64-shared.tar.bz2" },
#     "hashes":  { "macos": "<sha256>", "windows-x64": "<sha256>" }
#   }

include_guard(GLOBAL)

function(_fetch_sherpa_onnx platform_label)
  set(dest "${CMAKE_SOURCE_DIR}/.deps/sherpa-onnx")

  if(EXISTS "${dest}/include/sherpa-onnx/c-api/c-api.h")
    # Already extracted — allow re-runs to be cheap.
    message(STATUS "sherpa-onnx already present at ${dest}, skipping download")
    return()
  endif()

  file(READ "${CMAKE_SOURCE_DIR}/buildspec.json" buildspec)
  string(JSON sherpa GET "${buildspec}" dependencies "sherpa-onnx")
  string(JSON version GET "${sherpa}" version)
  string(JSON base_url GET "${sherpa}" baseUrl)
  string(JSON asset GET "${sherpa}" assets "${platform_label}")
  string(JSON expected_hash ERROR_VARIABLE hash_err GET "${sherpa}" hashes "${platform_label}")

  set(download_url "${base_url}/v${version}/${asset}")
  set(archive_path "${CMAKE_SOURCE_DIR}/.deps/${asset}")

  file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.deps")

  if(NOT EXISTS "${archive_path}")
    message(STATUS "Downloading sherpa-onnx ${version} (${platform_label}): ${download_url}")
    if(expected_hash AND NOT expected_hash STREQUAL "")
      file(DOWNLOAD "${download_url}" "${archive_path}"
        STATUS download_status
        EXPECTED_HASH SHA256=${expected_hash})
    else()
      file(DOWNLOAD "${download_url}" "${archive_path}" STATUS download_status)
    endif()
    list(GET download_status 0 download_code)
    list(GET download_status 1 download_msg)
    if(NOT download_code EQUAL 0)
      file(REMOVE "${archive_path}")
      message(FATAL_ERROR "Failed to download sherpa-onnx: ${download_msg}")
    endif()
  endif()

  # Extract into a staging dir then flatten one level, because the upstream
  # archive has a single top-level directory like "sherpa-onnx-v1.12.39-...".
  set(staging "${CMAKE_SOURCE_DIR}/.deps/_sherpa_staging")
  file(REMOVE_RECURSE "${staging}")
  file(MAKE_DIRECTORY "${staging}")
  file(ARCHIVE_EXTRACT INPUT "${archive_path}" DESTINATION "${staging}")

  file(GLOB staging_children RELATIVE "${staging}" "${staging}/*")
  list(LENGTH staging_children child_count)
  if(child_count EQUAL 1)
    list(GET staging_children 0 top_dir)
    set(extracted_root "${staging}/${top_dir}")
  else()
    set(extracted_root "${staging}")
  endif()

  file(REMOVE_RECURSE "${dest}")
  file(MAKE_DIRECTORY "${dest}")

  # We only need include/ and lib/. Copy them verbatim.
  if(EXISTS "${extracted_root}/include")
    file(COPY "${extracted_root}/include" DESTINATION "${dest}")
  endif()
  if(EXISTS "${extracted_root}/lib")
    file(COPY "${extracted_root}/lib" DESTINATION "${dest}")
  endif()

  file(REMOVE_RECURSE "${staging}")
  message(STATUS "sherpa-onnx ${version} ready at ${dest}")
endfunction()
