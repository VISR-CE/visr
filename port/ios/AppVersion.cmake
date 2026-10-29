# Resolve Git metadata at configure time; rerun CMake to refresh the tag, SHA or dirty marker.
# Keep the automatic value out of the cache so a reused build directory refreshes it too.
set(HALO_MARKETING_VERSION "" CACHE STRING "CFBundleShortVersionString supplied by the CI release tag")
set(HALO_BUILD_DESCRIPTION "${HALO_MARKETING_VERSION}")
if(NOT HALO_MARKETING_VERSION)
    set(HALO_MARKETING_VERSION "0.1.2")
    set(HALO_BUILD_DESCRIPTION "${HALO_MARKETING_VERSION}")
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --match "visr-v*" --abbrev=0
            WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE tag_result
            OUTPUT_VARIABLE version_tag OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
            WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE sha_result
            OUTPUT_VARIABLE version_sha OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
            WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE status_result
            OUTPUT_VARIABLE version_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(tag_result EQUAL 0 AND version_tag MATCHES "^visr-v([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(HALO_MARKETING_VERSION "${CMAKE_MATCH_1}")
        endif()
        if(sha_result EQUAL 0 AND status_result EQUAL 0)
            string(SUBSTRING "${version_sha}" 0 9 version_sha)
            if(version_status)
                string(APPEND version_sha "+")
            endif()
            set(HALO_BUILD_DESCRIPTION "${HALO_MARKETING_VERSION}-dev (${version_sha})")
        endif()
    endif()
endif()
