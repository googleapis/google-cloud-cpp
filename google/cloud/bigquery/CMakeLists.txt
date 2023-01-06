# ~~~
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud BigQuery C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud BigQuery")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "bigquery_internal" "bigquery_testing"
                            "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::cloud_bigquery_protos)

include(GoogleCloudCppCommon)

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.h" "*.cc" "internal/*.h" "internal/*.cc")
list(SORT source_files)
add_library(google_cloud_cpp_bigquery ${source_files})
target_include_directories(
    google_cloud_cpp_bigquery
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_bigquery
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::cloud_bigquery_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigquery)
set_target_properties(
    google_cloud_cpp_bigquery
    PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquery
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::bigquery ALIAS google_cloud_cpp_bigquery)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources. target_sources example:
# ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_servicename_v2_connection.h
file(
    GLOB relative_mock_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "mocks/*.h")
list(SORT relative_mock_files)
set(mock_files)
foreach (file IN LISTS relative_mock_files)
    list(APPEND mock_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach ()
add_library(google_cloud_cpp_bigquery_mocks INTERFACE)
target_sources(google_cloud_cpp_bigquery_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_bigquery_mocks
    INTERFACE google-cloud-cpp::bigquery GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(google_cloud_cpp_bigquery_mocks
                      PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery_mocks)
target_include_directories(
    google_cloud_cpp_bigquery_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_bigquery_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(bigquery_quickstart "quickstart/quickstart.cc")
    target_link_libraries(bigquery_quickstart
                          PRIVATE google-cloud-cpp::bigquery)
    google_cloud_cpp_add_common_options(bigquery_quickstart)
    add_test(
        NAME bigquery_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:bigquery_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE)
    set_tests_properties(bigquery_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigquery-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_bigquery
    EXPORT google_cloud_cpp_bigquery-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_bigquery
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_bigquery"
                                 "include/google/cloud/bigquery")
google_cloud_cpp_install_headers("google_cloud_cpp_bigquery_mocks"
                                 "include/google/cloud/bigquery")

google_cloud_cpp_add_pkgconfig(
    bigquery
    "The Google Cloud BigQuery C++ Client Library"
    "Provides C++ APIs to access Google Cloud BigQuery."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_cloud_bigquery_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_bigquery-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigquery-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery"
    COMPONENT google_cloud_cpp_development)

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    google_cloud_cpp_add_samples_relative("bigquery" "samples/")
    target_link_libraries(bigquery_samples_mock_bigquery_read
                          PRIVATE GTest::gmock_main)
endif ()
