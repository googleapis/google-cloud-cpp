# ~~~
# Copyright 2019 Google LLC
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
set(GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MAJOR 0)
set(GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MINOR 1)
set(GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_PATCH 0)
string(CONCAT GRPC_UTILS_VERSION
              "${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MAJOR}"
              "."
              "${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MINOR}"
              "."
              "${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_PATCH}")

set(DOXYGEN_PROJECT_NAME "Google Cloud C++ gRPC Utilities")
set(DOXYGEN_PROJECT_BRIEF
    "gRPC Utilities for the Google Cloud C++ Client Library")
set(DOXYGEN_PROJECT_NUMBER "${GRPC_UTILS_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(
    DOXYGEN_PREDEFINED
    "GOOGLE_CLOUD_CPP_GRPC_UTILS_NS=v${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MAJOR}"
    )
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/grpc_utils/README.md"
    "*/google/cloud/grpc_utils/ci/*"
    "*/google/cloud/grpc_utils/internal/*"
    "*/google/cloud/grpc_utils/testing/*"
    "*/google/cloud/grpc_utils/tests/*"
    "*/google/cloud/grpc_utils/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
set(DOXYGEN_TAGFILES "${PROJECT_BINARY_DIR}/google/cloud/cloud.tag=../common")
google_cloud_cpp_set_doxygen_tagfiles()

include(GoogleCloudCppCommon)
if (TARGET grpc_utils-docs AND TARGET cloud-docs)
    add_dependencies(grpc_utils-docs cloud-docs)
endif ()

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE grpc_utils_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(grpc_utils_common_options INTERFACE)
google_cloud_cpp_add_common_options(grpc_utils_common_options)

include(external/googleapis)

# Enable unit tests
include(CTest)

# Generate the version information from the CMake values.
configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)

# the library
add_library(google_cloud_cpp_grpc_utils
            grpc_error_delegate.h
            grpc_error_delegate.cc
            version.h
            version.cc
            version_info.h)
target_link_libraries(google_cloud_cpp_grpc_utils
                      PUBLIC googleapis-c++::rpc_status_protos
                             google_cloud_cpp_common
                             gRPC::grpc++
                             gRPC::grpc
                      PRIVATE grpc_utils_common_options)
target_include_directories(google_cloud_cpp_grpc_utils
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_grpc_utils
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(google_cloud_cpp_grpc_utils
                      PROPERTIES VERSION
                                 ${GRPC_UTILS_VERSION}
                                 SOVERSION
                                 ${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MAJOR})

include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_grpc_utils YEAR 2019)
google_cloud_cpp_add_clang_tidy(google_cloud_cpp_grpc_utils)

if (BUILD_TESTING)
    # TODO: add google_cloud_cpp_grpc_utils_testing if necessary.

    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_grpc_utils_unit_tests grpc_error_delegate_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_grpc_utils_unit_tests.bzl"
                         "google_cloud_cpp_grpc_utils_unit_tests"
                         YEAR
                         2019)

    foreach (fname ${google_cloud_cpp_grpc_utils_unit_tests})
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
                              PRIVATE google_cloud_cpp_grpc_utils
                                      google_cloud_cpp_testing
                                      google_cloud_cpp_common
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest
                                      gRPC::grpc++
                                      gRPC::grpc
                                      grpc_utils_common_options)
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

option(GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS
       "If set, enable tests that force errors detected by the static analyzer."
       "")
mark_as_advanced(GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS)
if (GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(
        google_cloud_cpp_grpc_utils_force_sanitizer_failures_test PRIVATE
        -DGOOGLE_CLOUD_CPP_GRPC_UTILS_FORCE_STATIC_ANALYZER_ERRORS)
endif ()

# TODO: add_subdirectory(tests) if necessary.

if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "external")
    set(GOOGLE_CLOUD_CPP_GRPC_UTILS_ENABLE_INSTALL "OFF")
    message(
        WARNING
            "The install target has been disabled for the google_cloud_cpp_grpc_utils"
            " library because one of the dependencies is set to use external"
            " projects. When building against external projects the resulting"
            " libraries are not installable. The necessary dependencies may"
            " not installed in your system, or the versions installed in your"
            " system may not match the version used to compile the"
            " google-cloud-cpp libraries. "
            ""
            "Please consult the README.md file for details on how to compile "
            "against pre-installed versions of the google-cloud-cpp "
            "dependencies.
GPRC     PROVIDER = ${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}
PROTOBUF PROVIDER = ${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}")
else()
    set(GOOGLE_CLOUD_CPP_GRPC_UTILS_ENABLE_INSTALL "ON")
endif ()

if ("${GOOGLE_CLOUD_CPP_GRPC_UTILS_ENABLE_INSTALL}")
    # Install the libraries and headers in the locations determined by
    # GNUInstallDirs
    install(TARGETS grpc_utils_common_options
            EXPORT grpc_utils-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Export the CMake targets to make it easy to create configuration files.
    install(EXPORT grpc_utils-targets
            DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_grpc_utils")

    install(TARGETS google_cloud_cpp_grpc_utils
            EXPORT grpc_utils-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    google_cloud_cpp_install_headers(google_cloud_cpp_grpc_utils
                                     include/google/cloud/grpc_utils)

    # Setup global variables used in the following *.in files.
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR
        ${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MAJOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR
        ${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_MINOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH
        ${GOOGLE_CLOUD_CPP_GRPC_UTILS_VERSION_PATCH})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "gRPC Utilities for the Google Cloud C++ Client Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
        "Provides gRPC Utilities for the Google Cloud C++ Client Library.")
    set(GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_common")
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_grpc_utils")

    # Create and install the pkg-config files.
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
                   "google_cloud_cpp_grpc_utils.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils.pc"
                  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Create and install the CMake configuration files.
    configure_file("config.cmake.in" "google_cloud_cpp_grpc_utils-config.cmake"
                   @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
                   "google_cloud_cpp_grpc_utils-config-version.cmake" @ONLY)
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils-config-version.cmake"
            "${PROJECT_SOURCE_DIR}/cmake/FindgRPC.cmake"
            "${PROJECT_SOURCE_DIR}/cmake/FindProtobufTargets.cmake"
            "${PROJECT_SOURCE_DIR}/cmake/PkgConfigHelper.cmake"
            DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_grpc_utils")
endif ()
