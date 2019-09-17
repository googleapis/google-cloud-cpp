# ~~~
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# This should be included from the top level CMakeLists file
set(GOOGLE_CLOUD_CPP_VERSION_MAJOR 0)
set(GOOGLE_CLOUD_CPP_VERSION_MINOR 12)
set(GOOGLE_CLOUD_CPP_VERSION_PATCH 0)

string(CONCAT GOOGLE_CLOUD_CPP_VERSION
              "${GOOGLE_CLOUD_CPP_VERSION_MAJOR}"
              "."
              "${GOOGLE_CLOUD_CPP_VERSION_MINOR}"
              "."
              "${GOOGLE_CLOUD_CPP_VERSION_PATCH}")

# Generate the version information from the CMake values.
configure_file(internal/version_info.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/internal/version_info.h)

set(DOXYGEN_PROJECT_NAME "Google Cloud C++ Client")
set(DOXYGEN_PROJECT_BRIEF "C++ Client Library for Google Cloud Platform")
set(
    DOXYGEN_PROJECT_NUMBER
    "${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}"
    )
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v${GOOGLE_CLOUD_CPP_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/README.md"
    "*/google/cloud/internal/*"
    "*/google/cloud/testing_util/*"
    "*/google/cloud/bigtable/*"
    "*/google/cloud/firestore/*"
    "*/google/cloud/spanner/*"
    "*/google/cloud/storage/*"
    "*/google/cloud/grpc_utils/*"
    "*/google/cloud/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOL "internal")

include(GoogleCloudCppCommon)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE bigtable_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(google_cloud_cpp_common_options INTERFACE)
google_cloud_cpp_add_common_options(google_cloud_cpp_common_options)

# Define a function to fetch the current git revision. Using a function creates
# a new scope, so the CMake variables do not leak to the global namespace.
function (google_cloud_cpp_initialize_git_head var)
    set(result "")
    # If we cannot find a `.git` directory do not even try to guess the git
    # revision.
    if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
        set(result "unknown-commit")
        # We need `git` to find the revision.
        find_program(GOOGLE_CLOUD_CPP_GIT_PROGRAM NAMES git)
        mark_as_advanced(GOOGLE_CLOUD_CPP_GIT_PROGRAM)
        if (GOOGLE_CLOUD_CPP_GIT_PROGRAM)
            # Run `git rev-parse --short HEAD` and capture the output in a
            # variable.
            execute_process(COMMAND "${GOOGLE_CLOUD_CPP_GIT_PROGRAM}" rev-parse
                                    --short HEAD
                            OUTPUT_VARIABLE GIT_HEAD_LOG
                            ERROR_VARIABLE GIT_HEAD_LOG)
            string(REPLACE "\n"
                           ""
                           result
                           "${GIT_HEAD_LOG}")
        endif ()
    endif ()
    set(${var} "${result}" PARENT_SCOPE)
endfunction ()

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (NOT "${GOOGLE_CLOUD_CPP_BUILD_METADATA}" STREQUAL "")
    # The build metadata flag is already defined, do not re-compute the
    # initialization value. This works both when the user supplies
    # -DGOOGLE_CLOUD_CPP_METADATA=value in the command line, and when
    # GOOGLE_CLOUD_CPP_METADATA has a cached value
    set(GOOGLE_CLOUD_CPP_GIT_HEAD "unused")
else()
    google_cloud_cpp_initialize_git_head(GOOGLE_CLOUD_CPP_GIT_HEAD)
endif ()

# Define a CMake configuration option to set the build metadata. By default this
# is initialized from `git rev-parse --short HEAD`, but the developer (or the
# script building via CMake) can override the value.
set(GOOGLE_CLOUD_CPP_BUILD_METADATA "${GOOGLE_CLOUD_CPP_GIT_HEAD}"
    CACHE STRING "Append build metadata to the library version number")
# This option is rarely needed. Mark it as "advanced" to remove it from the
# default CMake UIs.
mark_as_advanced(GOOGLE_CLOUD_CPP_BUILD_METADATA)

message(STATUS "google-cloud-cpp build metadata set to"
               " ${GOOGLE_CLOUD_CPP_BUILD_METADATA}")

# Create the file that captures build information. Having access to the compiler
# and build flags at runtime allows us to print better benchmark results.
string(TOUPPER "${CMAKE_BUILD_TYPE}" GOOGLE_CLOUD_CPP_BUILD_TYPE_UPPER)
configure_file(internal/build_info.cc.in internal/build_info.cc)

# the client library
add_library(google_cloud_cpp_common
            future.h
            future_generic.h
            future_void.h
            iam_binding.h
            iam_bindings.h
            iam_bindings.cc
            iam_policy.h
            iam_policy.cc
            internal/backoff_policy.h
            internal/backoff_policy.cc
            internal/big_endian.h
            internal/build_info.h
            ${CMAKE_CURRENT_BINARY_DIR}/internal/build_info.cc
            internal/compiler_info.h
            internal/compiler_info.cc
            internal/conjunction.h
            internal/disjunction.h
            internal/filesystem.h
            internal/filesystem.cc
            internal/format_time_point.h
            internal/format_time_point.cc
            internal/future_base.h
            internal/future_fwd.h
            internal/future_impl.h
            internal/future_impl.cc
            internal/future_then_impl.h
            internal/future_then_meta.h
            internal/getenv.h
            internal/getenv.cc
            internal/ios_flags_saver.h
            internal/make_unique.h
            internal/parse_rfc3339.h
            internal/parse_rfc3339.cc
            internal/port_platform.h
            internal/random.h
            internal/random.cc
            internal/invoke_result.h
            internal/retry_policy.h
            internal/setenv.h
            internal/setenv.cc
            internal/throw_delegate.h
            internal/throw_delegate.cc
            internal/version_info.h
            log.h
            log.cc
            optional.h
            status.h
            status.cc
            status_or.h
            terminate_handler.h
            terminate_handler.cc
            version.h)
target_link_libraries(google_cloud_cpp_common
                      PUBLIC Threads::Threads
                      PRIVATE google_cloud_cpp_common_options)
target_include_directories(google_cloud_cpp_common
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_common
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
if (MSVC)
    # MSVC warns about using sscanf(), this is a valuable warning, so we do not
    # want to disable for everything. But for these files the usage is "safe".
    # This is a relatively obscure feature to add compile flags to just one
    # source:
    set_property(SOURCE internal/parse_rfc3339.cc
                 APPEND_STRING
                 PROPERTY COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
endif ()

set_target_properties(google_cloud_cpp_common
                      PROPERTIES VERSION
                                 ${GOOGLE_CLOUD_CPP_VERSION}
                                 SOVERSION
                                 ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})

include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_common)
google_cloud_cpp_add_clang_tidy(google_cloud_cpp_common)

add_subdirectory(testing_util)

if (BUILD_TESTING)
    set(google_cloud_cpp_common_unit_tests
        future_generic_test.cc
        future_generic_then_test.cc
        future_void_test.cc
        future_void_then_test.cc
        iam_bindings_test.cc
        internal/backoff_policy_test.cc
        internal/big_endian_test.cc
        internal/compiler_info_test.cc
        internal/filesystem_test.cc
        internal/format_time_point_test.cc
        internal/future_impl_test.cc
        internal/invoke_result_test.cc
        internal/parse_rfc3339_test.cc
        internal/random_test.cc
        internal/retry_policy_test.cc
        internal/throw_delegate_test.cc
        internal/env_test.cc
        log_test.cc
        optional_test.cc
        status_or_test.cc
        status_test.cc
        terminate_handler_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_common_unit_tests.bzl"
                         "google_cloud_cpp_common_unit_tests")

    foreach (fname ${google_cloud_cpp_common_unit_tests})
        string(REPLACE "/"
                       "_"
                       target
                       ${fname})
        string(REPLACE ".cc"
                       ""
                       target
                       ${target})
        add_executable(${target} ${fname})
        target_link_libraries(${target}
                              PRIVATE google_cloud_cpp_testing
                                      google_cloud_cpp_common
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest
                                      google_cloud_cpp_common_options)
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT google_cloud_cpp_common-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS google_cloud_cpp_common google_cloud_cpp_common_options
        EXPORT google_cloud_cpp_common-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
google_cloud_cpp_install_headers(google_cloud_cpp_common include/google/cloud)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${GOOGLE_CLOUD_CPP_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${GOOGLE_CLOUD_CPP_VERSION_PATCH})
set(
    GOOGLE_CLOUD_CPP_PC_NAME "Google Cloud C++ Client Library Common Components"
    )
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Common Components used by the Google Cloud C++ Client Libraries.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_common")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "google_cloud_cpp_common.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common.pc"
              DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "google_cloud_cpp_common-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
               "google_cloud_cpp_common-config-version.cmake" @ONLY)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config-version.cmake"
        DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common")

if (GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE)
    add_subdirectory(bigtable)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_FIRESTORE)
    add_subdirectory(firestore)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    add_subdirectory(storage)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_GRPC_UTILS)
    add_subdirectory(grpc_utils)
endif ()

# The examples are more readable if we use exceptions for error handling. We had
# to tradeoff readability vs. "making them compile everywhere".
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    # The examples require these libraries to be enabled.
    if (GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE
        AND GOOGLE_CLOUD_CPP_ENABLE_STORAGE
        AND GOOGLE_CLOUD_CPP_ENABLE_GRPC_UTILS)
        add_subdirectory(examples)
    endif ()
endif ()
