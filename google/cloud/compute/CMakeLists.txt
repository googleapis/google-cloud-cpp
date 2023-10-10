# ~~~
# Copyright 2023 Google LLC
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

# Defines a compute proto library and installs the .proto and .pb.h files.
function (compute_proto_library dir library)
    cmake_parse_arguments(_opt "" "" "DEPS" ${ARGN})
    file(
        GLOB proto_files
        LIST_DIRECTORIES false
        RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}*.proto")
    list(SORT proto_files)
    string(REPLACE "google_cloud_cpp_" "" alias "${library}")
    google_cloud_cpp_proto_library(
        "${library}" "${proto_files}" PROTO_PATH_DIRECTORIES
        "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}"
        "${PROJECT_SOURCE_DIR}")
    external_googleapis_set_version_and_alias("${alias}")
    target_link_libraries("${library}" PUBLIC "${_opt_DEPS}")
    # Some files are too big for the MSVC defaults.
    if (MSVC)
        target_compile_options("${library}" PRIVATE "/bigobj")
    endif ()
    google_cloud_cpp_install_proto_library_protos("${library}"
                                                  "${PROJECT_SOURCE_DIR}")
    google_cloud_cpp_install_proto_library_headers("${library}")
endfunction ()

# Defines a compute service library and installs the headers.
function (compute_service_library dir library)
    cmake_parse_arguments(_opt "" "" "DEPS" ${ARGN})
    file(
        GLOB service_source_files
        LIST_DIRECTORIES false
        RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}*.h" "${dir}*.cc"
        "${dir}internal/*")
    list(SORT service_source_files)

    string(REPLACE "google_cloud_cpp_" "" alias "${library}")
    add_library(${library} ${service_source_files})
    target_include_directories(
        ${library}
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)

    target_link_libraries(${library} PUBLIC ${_opt_DEPS})

    google_cloud_cpp_add_common_options(${library})
    set_target_properties(
        ${library}
        PROPERTIES EXPORT_NAME google-cloud-cpp::${alias}
                   VERSION "${PROJECT_VERSION}"
                   SOVERSION "${PROJECT_VERSION_MAJOR}")
    target_compile_options(${library}
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    google_cloud_cpp_install_headers(${library} "include/google/cloud/compute")

    add_library(google-cloud-cpp::${alias} ALIAS ${library})
endfunction ()

include(GoogleapisConfig)
include(CreateBazelConfig)
include(${CMAKE_CURRENT_SOURCE_DIR}/service_dirs.cmake)
export_list_to_bazel("service_dirs.bzl" YEAR 2023 service_dirs
                     operation_service_dirs)

include(GoogleCloudCppCommon)
include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
google_cloud_cpp_load_protodeps(
    deps "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/compute.deps")

compute_proto_library("v1/internal/" "google_cloud_cpp_compute_internal_protos"
                      DEPS "${deps}")

foreach (dir IN LISTS service_dirs operation_service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    compute_proto_library(
        ${dir} "google_cloud_cpp_compute_${short_dir}_protos" PROTO_FILES
        "${${short_dir}_proto_files}" DEPS
        google_cloud_cpp_compute_internal_protos)
    list(APPEND compute_proto_lib_targets
         "google_cloud_cpp_compute_${short_dir}_protos")
endforeach ()

set(DOXYGEN_PROJECT_NAME "Cloud Compute API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the Cloud Compute API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Experimental)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
foreach (dir IN LISTS service_dirs operation_service_dirs)
    list(APPEND DOXYGEN_EXAMPLE_PATH
         "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
endforeach ()
list(APPEND DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/samples"
     "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")
include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("compute" THREADED DEPENDS cloud-docs
                                 ${compute_proto_lib_targets})

foreach (dir IN LISTS operation_service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    compute_service_library(
        ${dir} google_cloud_cpp_compute_${short_dir} DEPS
        google-cloud-cpp::rest_protobuf_internal
        google-cloud-cpp::compute_${short_dir}_protos)
    list(APPEND compute_operation_lib_targets
         "google_cloud_cpp_compute_${short_dir}")
endforeach ()

foreach (dir IN LISTS service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    compute_service_library(
        ${dir}
        google_cloud_cpp_compute_${short_dir}
        DEPS
        google-cloud-cpp::compute_${short_dir}_protos
        google-cloud-cpp::compute_global_operations
        google-cloud-cpp::compute_global_organization_operations
        google-cloud-cpp::compute_region_operations
        google-cloud-cpp::compute_zone_operations)
    list(APPEND compute_lib_targets "google_cloud_cpp_compute_${short_dir}")
endforeach ()

add_library(google_cloud_cpp_compute INTERFACE)
target_link_libraries(
    google_cloud_cpp_compute
    PUBLIC
    INTERFACE ${compute_lib_targets})
set_target_properties(google_cloud_cpp_compute
                      PROPERTIES EXPORT_NAME "google-cloud-cpp::compute")
add_library(google-cloud-cpp::compute ALIAS google_cloud_cpp_compute)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
foreach (dir IN LISTS service_dirs operation_service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    file(
        GLOB ${short_dir}_relative_mock_files
        LIST_DIRECTORIES false
        RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}mocks/*.h")
    list(SORT ${short_dir}_relative_mock_files)
    set(mock_files)
    foreach (file IN LISTS ${short_dir}_relative_mock_files)
        list(APPEND mock_files "${file}")
    endforeach ()
    add_library(google_cloud_cpp_compute_${short_dir}_mocks INTERFACE)
    target_sources(google_cloud_cpp_compute_${short_dir}_mocks
                   INTERFACE ${mock_files})
    target_link_libraries(
        google_cloud_cpp_compute_${short_dir}_mocks
        INTERFACE google-cloud-cpp::compute-${short_dir} GTest::gmock_main
                  GTest::gmock GTest::gtest)
    set_target_properties(
        google_cloud_cpp_compute_${short_dir}_mocks
        PROPERTIES EXPORT_NAME google-cloud-cpp::compute-${short_dir}_mocks)
    target_include_directories(
        google_cloud_cpp_compute_${short_dir}_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_compute_${short_dir}_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    google_cloud_cpp_install_headers(
        "google_cloud_cpp_compute_${short_dir}_mocks"
        "include/google/cloud/compute")
endforeach ()

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(compute_quickstart "quickstart/quickstart.cc")
    target_link_libraries(compute_quickstart
                          PRIVATE google-cloud-cpp::compute_disks)
    google_cloud_cpp_add_common_options(compute_quickstart)
    add_test(
        NAME compute_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:compute_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_ZONE)
    set_tests_properties(compute_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
    add_subdirectory(integration_tests)
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_compute-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_compute ${compute_lib_targets}
            ${compute_operation_lib_targets} ${compute_proto_lib_targets}
            google_cloud_cpp_compute_internal_protos
    EXPORT google_cloud_cpp_compute-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_compute-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_compute-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute"
    COMPONENT google_cloud_cpp_development)

google_cloud_cpp_add_pkgconfig(
    "compute_internal_protos" "The Cloud Compute API C++ Client Library"
    "Provides C++ APIs to use the Cloud Compute API."
    "google_cloud_cpp_cloud_extended_operations_protos")

foreach (dir IN LISTS service_dirs operation_service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    google_cloud_cpp_add_pkgconfig(
        "compute_${short_dir}_protos"
        "The Cloud Compute API C++ Client Library"
        "Provides C++ APIs to use the Cloud Compute API."
        "google_cloud_cpp_compute_internal_protos"
        "google_cloud_cpp_api_annotations_protos"
        "google_cloud_cpp_api_client_protos"
        "google_cloud_cpp_api_field_behavior_protos")
endforeach ()

foreach (dir IN LISTS operation_service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    google_cloud_cpp_add_pkgconfig(
        "compute_${short_dir}"
        "The Cloud Compute API C++ Client Library"
        "Provides C++ APIs to use the Cloud Compute API."
        "google_cloud_cpp_rest_internal"
        "google_cloud_cpp_rest_protobuf_internal"
        "google_cloud_cpp_grpc_utils"
        "google_cloud_cpp_common"
        "google_cloud_cpp_compute_${short_dir}_protos")
endforeach ()

foreach (dir IN LISTS service_dirs)
    string(REPLACE "/v1/" "" short_dir "${dir}")
    google_cloud_cpp_add_pkgconfig(
        "compute_${short_dir}"
        "The Cloud Compute API C++ Client Library"
        "Provides C++ APIs to use the Cloud Compute API."
        "google_cloud_cpp_compute_global_operations"
        "google_cloud_cpp_compute_global_organization_operations"
        "google_cloud_cpp_compute_region_operations"
        "google_cloud_cpp_compute_zone_operations"
        "google_cloud_cpp_compute_${short_dir}_protos")
endforeach ()

google_cloud_cpp_add_pkgconfig(
    "compute" "The Cloud Compute API C++ Client Library"
    "Provides C++ APIs to use the Cloud Compute API." ${compute_lib_targets})
