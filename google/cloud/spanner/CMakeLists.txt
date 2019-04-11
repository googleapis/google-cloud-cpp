# ~~~
# Copyright 2019 Google Inc.
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

include(external/googleapis)

# This should be included from the top level CMakeLists file
set(SPANNER_CLIENT_VERSION_MAJOR 0)
set(SPANNER_CLIENT_VERSION_MINOR 1)
set(SPANNER_CLIENT_VERSION_PATCH 0)

set(DOXYGEN_PROJECT_NAME "Google Cloud Spanner C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Spanner")
set(
    DOXYGEN_PROJECT_NUMBER
    "${SPANNER_CLIENT_VERSION_MAJOR}.${SPANNER_CLIENT_VERSION_MINOR}.${SPANNER_CLIENT_VERSION_PATCH}"
    )
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(DOXYGEN_PREDEFINED "SPANNER_CLIENT_NS=v${SPANNER_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/spanner/README.md"
    "*/google/cloud/spanner/*_test.cc")
set(DOXYGEN_TAGFILES "${PROJECT_BINARY_DIR}/google/cloud/cloud.tag=../common")

include(GoogleCloudCppCommon)
if (TARGET spanner-docs AND TARGET cloud-docs)
    add_dependencies(spanner-docs cloud-docs)
endif ()

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE spanner_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(spanner_common_options INTERFACE)
google_cloud_cpp_add_common_options(spanner_common_options)

# Enable unit tests
include(CTest)

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)
export_variables_to_bazel("spanner_client_version.bzl"
                          SPANNER_CLIENT_VERSION_MAJOR
                          SPANNER_CLIENT_VERSION_MINOR
                          SPANNER_CLIENT_VERSION_PATCH)

# the client library
add_library(spanner_client
            ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
            version.h
            version.cc)
target_link_libraries(spanner_client
                      PUBLIC google_cloud_cpp_common
                      PRIVATE spanner_common_options)
target_include_directories(spanner_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(spanner_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    spanner_client
    PROPERTIES
        VERSION
        ${SPANNER_CLIENT_VERSION_MAJOR}.${SPANNER_CLIENT_VERSION_MINOR}.${SPANNER_CLIENT_VERSION_PATCH}
        SOVERSION
        ${SPANNER_CLIENT_VERSION_MAJOR})
add_library(spanner::client ALIAS spanner_client)

include(CreateBazelConfig)
create_bazel_config(spanner_client)
google_cloud_cpp_add_clang_tidy(spanner_client)

if (BUILD_TESTING)
    add_library(spanner_client_testing
                # TODO: 'git rm' this file when we have a real implementation.
                spanner_client_testing_placeholder.h)
    target_link_libraries(spanner_client_testing
                          PUBLIC spanner_client
                                 GTest::gmock_main
                                 GTest::gmock
                                 GTest::gtest
                          PRIVATE spanner_common_options)

    create_bazel_config(spanner_client_testing)

    # List the unit tests, then setup the targets and dependencies.
    set(spanner_client_unit_tests
        spanner_version_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("spanner_client_unit_tests.bzl"
                         "spanner_client_unit_tests")

    foreach (fname ${spanner_client_unit_tests})
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
                              PRIVATE spanner_client_testing
                                      spanner_client
                                      google_cloud_cpp_testing
                                      google_cloud_cpp_common
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest
                                      spanner_common_options)
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

option(FORCE_SANITIZER_ERRORS
       "If set, enable tests that force errors detected by the sanitizers." "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(spanner_client_force_sanitizer_failures_test
                               PRIVATE -DSPANNER_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

option(FORCE_STATIC_ANALYZER_ERRORS
       "If set, enable tests that force errors detected by the static analyzer."
       "")
if (FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(spanner_client_force_sanitizer_failures_test
                               PRIVATE
                               -DSPANNER_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif (FORCE_STATIC_ANALYZER_ERRORS)

set(GOOGLE_CLOUD_CPP_SPANNER_ENABLE_INSTALL "ON")

if ("${GOOGLE_CLOUD_CPP_SPANNER_ENABLE_INSTALL}")
    # Install the libraries and headers in the locations determined by
    # GNUInstallDirs
    install(TARGETS spanner_common_options
            EXPORT spanner-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Export the CMake targets to make it easy to create configuration files.
    install(EXPORT spanner-targets
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/spanner_client")

    install(TARGETS spanner_client
            EXPORT spannerj-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    google_cloud_cpp_install_headers(spanner_client
                                     include/google/cloud/spanner)

    # Setup global variables used in the following *.in files.
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${SPANNER_CLIENT_VERSION_MAJOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${SPANNER_CLIENT_VERSION_MINOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${SPANNER_CLIENT_VERSION_PATCH})
    set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Spanner C++ Client Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
        "Provides C++ APIs to access Google Cloud Spanner.")
    set(GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_common googleapis")
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-lspanner_client")

    # Create and install the pkg-config files.
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
                   "spanner_client.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/spanner_client.pc" DESTINATION
                  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Create and install the CMake configuration files.
    configure_file("config.cmake.in" "spanner_client-config.cmake" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
                   "spanner_client-config-version.cmake" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/spanner_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/spanner_client-config-version.cmake"
              DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/cmake/spanner_client")
endif ()
