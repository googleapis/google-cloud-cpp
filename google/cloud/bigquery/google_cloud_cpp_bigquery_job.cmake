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
include(GoogleapisConfig)

unset(mocks_globs)
unset(source_globs)
set(service_dirs "job/v2/")
foreach (dir IN LISTS service_dirs)
    string(REPLACE "/" "_" ns "${dir}")
    list(APPEND source_globs "${dir}*.h" "${dir}internal/*.h"
         "${dir}internal/*_sources.cc")
    list(APPEND mocks_globs "${dir}mocks/*.h")
endforeach ()

include(GoogleCloudCppCommon)

include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/bigquery_v2.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/bigquery_v2.deps")
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
add_library(google_cloud_cpp_bigquery_job ${source_files})
target_include_directories(
    google_cloud_cpp_bigquery_job
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_bigquery_job
    PUBLIC google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::rest_internal
           google-cloud-cpp::rest_protobuf_internal
           google-cloud-cpp::bigquery_v2_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigquery_job)
set_target_properties(
    google_cloud_cpp_bigquery_job
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-bigquery_job
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquery_job
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-bigquery_job ALIAS
            google_cloud_cpp_bigquery_job)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_bigquery_job YEAR "2024")

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigquery_job-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery_job"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_bigquery_job google_cloud_cpp_bigquery_v2_protos
    EXPORT google_cloud_cpp_bigquery_job-targets
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
google_cloud_cpp_install_headers("google_cloud_cpp_bigquery_job"
                                 "include/google/cloud/bigquery")

google_cloud_cpp_add_pkgconfig(
    bigquery_job
    "The Cloud BigQuery Job API C++ Client Library"
    "Provides C++ APIs to use the Cloud BigQuery Job API."
    "google_cloud_cpp_rest_internal"
    "google_cloud_cpp_rest_protobuf_internal"
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_common"
    "google_cloud_cpp_bigquery_v2_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config-job.cmake.in"
               "google_cloud_cpp_bigquery_job-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigquery_job-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery_job-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery_job-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery_job"
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
    add_library(google_cloud_cpp_bigquery_job_mocks INTERFACE)
    target_sources(google_cloud_cpp_bigquery_job_mocks INTERFACE ${mock_files})
    target_link_libraries(
        google_cloud_cpp_bigquery_job_mocks
        INTERFACE google-cloud-cpp::bigquery_job GTest::gmock_main GTest::gmock
                  GTest::gtest)
    set_target_properties(
        google_cloud_cpp_bigquery_job_mocks
        PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery_job_mocks)
    target_include_directories(
        google_cloud_cpp_bigquery_job_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_bigquery_job_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    # google_cloud_cpp_install_mocks(bigquery_job "Cloud BigQuery Job API")
    function (google_cloud_cpp_install_bigquery_mocks library install_dir
              display_name)
        set(library_target "google_cloud_cpp_${library}")
        set(mocks_target "google_cloud_cpp_${library}_mocks")
        install(
            EXPORT ${mocks_target}-targets
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${mocks_target}"
            COMPONENT google_cloud_cpp_development)
        install(
            TARGETS ${mocks_target}
            EXPORT ${mocks_target}-targets
            COMPONENT google_cloud_cpp_development)

        google_cloud_cpp_install_headers("${mocks_target}"
                                         "include/google/cloud/${install_dir}")

        google_cloud_cpp_add_pkgconfig(
            ${library}_mocks "${display_name} Mocks"
            "Mocks for the ${display_name} C++ Client Library"
            "${library_target}" "gmock_main")

        set(GOOGLE_CLOUD_CPP_CONFIG_LIBRARY "${library_target}")
        configure_file(
            "${PROJECT_SOURCE_DIR}/cmake/templates/mocks-config.cmake.in"
            "${mocks_target}-config.cmake" @ONLY)
        write_basic_package_version_file(
            "${mocks_target}-config-version.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY ExactVersion)

        install(
            FILES
                "${CMAKE_CURRENT_BINARY_DIR}/${mocks_target}-config.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/${mocks_target}-config-version.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${mocks_target}"
            COMPONENT google_cloud_cpp_development)
    endfunction ()

    google_cloud_cpp_install_bigquery_mocks(bigquery_job "bigquery"
                                            "Cloud BigQuery Job API")

endif ()

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(bigquery_quickstart_job "quickstart/quickstart_job.cc")
    target_link_libraries(bigquery_quickstart_job
                          PRIVATE google-cloud-cpp::experimental-bigquery_job)
    google_cloud_cpp_add_common_options(bigquery_quickstart_job)
    add_test(
        NAME bigquery_quickstart_job
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:bigquery_quickstart_job> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(bigquery_quickstart_job
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# google-cloud-cpp::bigquery_job must be defined before we can add the samples.
foreach (dir IN LISTS service_dirs)
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        google_cloud_cpp_add_samples_relative("bigquery_job" "${dir}samples/")
    endif ()
endforeach ()
