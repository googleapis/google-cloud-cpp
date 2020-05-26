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

set(DOXYGEN_PROJECT_NAME "Google Cloud BigQuery C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud BigQuery")
set(DOXYGEN_PROJECT_NUMBER "${GOOGLE_CLOUD_CPP_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples)

include(GoogleCloudCppCommon)

configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    bigquery_client # cmake-format: sort
    client.cc
    client.h
    connection.h
    connection_options.cc
    connection_options.h
    internal/connection_impl.cc
    internal/connection_impl.h
    internal/storage_stub.cc
    internal/storage_stub.h
    internal/stream_reader.h
    internal/streaming_read_result_source.cc
    internal/streaming_read_result_source.h
    read_result.h
    read_stream.cc
    read_stream.h
    row.h
    row_set.h
    version.h
    version_info.h)
target_include_directories(
    bigquery_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    bigquery_client PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
                           googleapis-c++::cloud_bigquery_protos)
google_cloud_cpp_add_common_options(bigquery_client)
set_target_properties(
    bigquery_client PROPERTIES VERSION "${GOOGLE_CLOUD_CPP_VERSION}"
                               SOVERSION "${GOOGLE_CLOUD_CPP_VERSION_MAJOR}")
target_compile_options(bigquery_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(googleapis-c++::bigquery_client ALIAS bigquery_client)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(bigquery_client YEAR "2019")

# Define the tests in a function so we have a new scope for variable names.
function (bigquery_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    add_library(
        bigquery_client_testing
        ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_storage_stub.h
        ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_storage_stub.cc)
    target_link_libraries(
        bigquery_client_testing
        PUBLIC googleapis-c++::bigquery_client google_cloud_cpp_testing
               GTest::gmock_main GTest::gmock GTest::gtest)
    google_cloud_cpp_add_common_options(bigquery_client_testing)
    create_bazel_config(bigquery_client_testing YEAR "2019")
    target_include_directories(
        bigquery_client_testing
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_compile_options(bigquery_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    set(bigquery_client_unit_tests # cmake-format: sort
                                   internal/connection_impl_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_client_unit_tests.bzl"
                         "bigquery_client_unit_tests" YEAR "2019")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(bigquery-client-tests)

    # Generate a target for each unit test.
    foreach (fname ${bigquery_client_unit_tests})
        google_cloud_cpp_add_executable(target "bigquery" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigquery_client_testing googleapis-c++::bigquery_client
                    google_cloud_cpp_testing GTest::gmock_main GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(bigquery-client-tests ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    bigquery_client_define_tests()
endif (BUILD_TESTING)

# Only compile the samples if we're building with exceptions enabled. They
# require exceptions to keep them simple and idiomatic.
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(samples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT bigquery-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bigquery_client")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS bigquery_client
    EXPORT bigquery-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_bigquery_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_bigquery_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_bigquery_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS bigquery_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_bigquery_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_bigquery_development)

google_cloud_cpp_install_headers("bigquery_client"
                                 "include/google/cloud/bigquery")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${GOOGLE_CLOUD_CPP_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${GOOGLE_CLOUD_CPP_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud BigQuery C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud BigQuery.")
set(GOOGLE_CLOUD_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common")
set(GOOGLE_CLOUD_PC_LIBS "-lbigquery_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/bigquery/config.pc.in"
               "bigquery_client.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/bigquery_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_bigquery_development)

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "bigquery_client-config.cmake" @ONLY)
configure_file(
    "${PROJECT_SOURCE_DIR}/google/cloud/bigquery/config-version.cmake.in"
    "bigquery_client-config-version.cmake" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/bigquery_client-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/bigquery_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bigquery_client"
    COMPONENT google_cloud_cpp_bigquery_development)
