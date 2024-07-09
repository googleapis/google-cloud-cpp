# ~~~
# Copyright 2024 Google LLC
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

include(GoogleCloudCppLibrary)

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Cloud BigQuery Control API C++ Client")
set(DOXYGEN_PROJECT_BRIEF
    "A C++ Client Library for the Cloud BigQuery Control API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Experimental)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

unset(mocks_globs)
unset(source_globs)
set(service_dirs
    # cmake-format: sort
    "dataset/v2/"
    "job/v2/"
    "model/v2/"
    "project/v2/"
    "routine/v2/"
    "row_access_policy/v2/"
    "table/v2/")
foreach (dir IN LISTS service_dirs)
    string(REPLACE "/" "_" ns "${dir}")
    list(APPEND source_globs "${dir}*.h" "${dir}internal/*.h"
         "${dir}internal/*_sources.cc")
    list(APPEND mocks_globs "${dir}mocks/*.h")
    list(APPEND DOXYGEN_EXAMPLE_PATH
         "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
    list(APPEND DOXYGEN_EXCLUDE_SYMBOLS "bigquerycontrol_${ns}internal")
endforeach ()

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("bigquerycontrol" DEPENDS cloud-docs
                                 google-cloud-cpp::bigquery_v2_protos)

include(GoogleCloudCppCommon)

include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/bigquerycontrol.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/bigquerycontrol.deps")
google_cloud_cpp_proto_library(
    google_cloud_cpp_bigquery_v2_protos # cmake-format: sort
    ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(bigquery_v2_protos)
target_link_libraries(google_cloud_cpp_bigquery_v2_protos PUBLIC ${proto_deps})

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${source_globs})
list(SORT source_files)
add_library(google_cloud_cpp_bigquerycontrol ${source_files})
target_include_directories(
    google_cloud_cpp_bigquerycontrol
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_bigquerycontrol
    PUBLIC google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::rest_internal
           google-cloud-cpp::rest_protobuf_internal
           google-cloud-cpp::bigquery_v2_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigquerycontrol)
set_target_properties(
    google_cloud_cpp_bigquerycontrol
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-bigquerycontrol
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquerycontrol
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-bigquerycontrol ALIAS
            google_cloud_cpp_bigquerycontrol)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigquerycontrol-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquerycontrol"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_bigquerycontrol google_cloud_cpp_bigquery_v2_protos
    EXPORT google_cloud_cpp_bigquerycontrol-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_bigquery_v2_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_bigquery_v2_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_bigquerycontrol"
                                 "include/google/cloud/bigquerycontrol")

google_cloud_cpp_add_pkgconfig(
    bigquerycontrol
    "The Cloud BigQuery Control API C++ Client Library"
    "Provides C++ APIs to use the Cloud BigQuery Control API."
    "google_cloud_cpp_rest_internal"
    "google_cloud_cpp_rest_protobuf_internal"
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_common"
    "google_cloud_cpp_bigquery_v2_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in"
               "google_cloud_cpp_bigquerycontrol-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigquerycontrol-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquerycontrol-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquerycontrol-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquerycontrol"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_bigquery_v2_protos")

if (GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Create a header-only library for the mocks. We use a CMake `INTERFACE`
    # library for these, a regular library would not work on macOS (where the
    # library needs at least one .o file). Unfortunately INTERFACE libraries are
    # a bit weird in that they need absolute paths for their sources.
    file(
        GLOB relative_mock_files
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${mocks_globs})
    list(SORT relative_mock_files)
    set(mock_files)
    foreach (file IN LISTS relative_mock_files)
        # We use a generator expression per the recommendation in:
        # https://stackoverflow.com/a/62465051
        list(APPEND mock_files
             "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${file}>")
    endforeach ()
    add_library(google_cloud_cpp_bigquerycontrol_mocks INTERFACE)
    target_sources(google_cloud_cpp_bigquerycontrol_mocks
                   INTERFACE ${mock_files})
    target_link_libraries(
        google_cloud_cpp_bigquerycontrol_mocks
        INTERFACE google-cloud-cpp::experimental-bigquerycontrol
                  GTest::gmock_main GTest::gmock GTest::gtest)
    set_target_properties(
        google_cloud_cpp_bigquerycontrol_mocks
        PROPERTIES EXPORT_NAME google-cloud-cpp::bigquerycontrol_mocks)
    target_include_directories(
        google_cloud_cpp_bigquerycontrol_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_bigquerycontrol_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    google_cloud_cpp_install_mocks(bigquerycontrol "Cloud BigQuery V2 API")
endif ()

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(bigquerycontrol_quickstart "quickstart/quickstart.cc")
    target_link_libraries(
        bigquerycontrol_quickstart
        PRIVATE google-cloud-cpp::experimental-bigquerycontrol)
    google_cloud_cpp_add_common_options(bigquerycontrol_quickstart)
    add_test(
        NAME bigquerycontrol_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:bigquerycontrol_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(bigquerycontrol_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# google-cloud-cpp::experimental-bigquerycontrol must be defined before we can
# add the samples.
foreach (dir IN LISTS service_dirs)
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        google_cloud_cpp_add_samples_relative("experimental-bigquerycontrol"
                                              "${dir}samples/")
    endif ()
endforeach ()
