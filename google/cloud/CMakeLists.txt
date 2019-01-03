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

# Set this variable to a true value for branches that are releases. That will
# remove the "+dev" suffix (technically "build metadata" according to
# semver.org). We only want that suffix on development branches.
set(GOOGLE_CLOUD_CPP_IS_RELEASE "yes")

# This should be included from the top level CMakeLists file
set(GOOGLE_CLOUD_CPP_VERSION_MAJOR 0)
set(GOOGLE_CLOUD_CPP_VERSION_MINOR 3)
set(GOOGLE_CLOUD_CPP_VERSION_PATCH 0)

# Generate the version information from the CMake values.
configure_file(internal/version_info.h.in internal/version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)
export_variables_to_bazel("google_cloud_cpp_common_version.bzl"
                          GOOGLE_CLOUD_CPP_IS_RELEASE
                          GOOGLE_CLOUD_CPP_VERSION_MAJOR
                          GOOGLE_CLOUD_CPP_VERSION_MINOR
                          GOOGLE_CLOUD_CPP_VERSION_PATCH)

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
    "*/google/cloud/bigtable/*"
    "*/google/cloud/firestore/*"
    "*/google/cloud/storage/*"
    "*/google/cloud/*_test.cc")

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

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if ("${GOOGLE_CLOUD_CPP_IS_RELEASE}")
    set(GOOGLE_CLOUD_CPP_GIT_HEAD "")
else()
    if (NOT IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
        set(GOOGLE_CLOUD_CPP_GIT_HEAD "unknown-commit")
    else()
        find_program(GIT_PROGRAM NAMES git)
        if ("${GIT_PROGRAM}" STREQUAL "GIT_PROGRAM-NOTFOUND")
            set(GOOGLE_CLOUD_CPP_GIT_HEAD "unknown-commit")
        else()
            execute_process(COMMAND "${GIT_PROGRAM}" rev-parse --short HEAD
                            OUTPUT_VARIABLE GIT_HEAD_LOG
                            ERROR_VARIABLE GIT_HEAD_LOG)
            string(REPLACE "\n"
                           ""
                           GOOGLE_CLOUD_CPP_GIT_HEAD
                           "${GIT_HEAD_LOG}")
        endif ()
    endif ()
endif ()

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
            internal/disjunction.h
            internal/filesystem.h
            internal/filesystem.cc
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
            internal/port_platform.h
            internal/random.h
            internal/random.cc
            internal/invoke_result.h
            internal/retry_policy.h
            internal/setenv.h
            internal/setenv.cc
            internal/throw_delegate.h
            internal/throw_delegate.cc
            ${CMAKE_CURRENT_BINARY_DIR}/internal/version_info.h
            log.h
            log.cc
            optional.h
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
set_target_properties(
    google_cloud_cpp_common
    PROPERTIES
        VERSION
        ${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}
        SOVERSION
        ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})

create_bazel_config(google_cloud_cpp_common)
google_cloud_cpp_add_clang_tidy(google_cloud_cpp_common)

if (BUILD_TESTING)
    add_library(google_cloud_cpp_testing
                testing_util/capture_log_lines_backend.h
                testing_util/capture_log_lines_backend.cc
                testing_util/check_predicate_becomes_false.h
                testing_util/chrono_literals.h
                testing_util/environment_variable_restore.h
                testing_util/environment_variable_restore.cc
                testing_util/expect_exception.h
                testing_util/expect_future_error.h
                testing_util/custom_google_mock_main.cc
                testing_util/init_google_mock.h
                testing_util/init_google_mock.cc
                testing_util/testing_types.h
                testing_util/testing_types.cc)
    target_link_libraries(google_cloud_cpp_testing
                          PUBLIC google_cloud_cpp_common GTest::gmock)

    create_bazel_config(google_cloud_cpp_testing)

    set(google_cloud_cpp_common_unit_tests
        future_generic_test.cc
        future_generic_then_test.cc
        future_void_test.cc
        future_void_then_test.cc
        iam_bindings_test.cc
        internal/backoff_policy_test.cc
        internal/big_endian_test.cc
        internal/filesystem_test.cc
        internal/future_impl_test.cc
        internal/invoke_result_test.cc
        internal/random_test.cc
        internal/retry_policy_test.cc
        internal/throw_delegate_test.cc
        log_test.cc
        optional_test.cc
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
        RUNTIME DESTINATION bin
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

add_subdirectory(bigtable)
add_subdirectory(firestore)
add_subdirectory(storage)
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(examples)
endif ()
