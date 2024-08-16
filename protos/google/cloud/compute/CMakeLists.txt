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

include(${PROJECT_SOURCE_DIR}/google/cloud/compute/service_dirs.cmake)

include(GoogleapisConfig)
include(CompileProtos)
include(GoogleCloudCppCommon)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
google_cloud_cpp_load_protodeps(
    deps "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/compute.deps")

set(proto_globs "v1/internal/" "${service_dirs}" "${operation_service_dirs}")
list(TRANSFORM proto_globs APPEND "*.proto")
file(
    GLOB proto_files
    LIST_DIRECTORIES false
    RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${proto_globs})
list(SORT proto_files)

google_cloud_cpp_proto_library(
    compute_protos "${proto_files}" PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}"
    "${PROJECT_SOURCE_DIR}/protos")
set_target_properties(compute_protos
                      PROPERTIES OUTPUT_NAME "google_cloud_cpp_compute_protos")
add_library(google-cloud-cpp::compute_protos ALIAS compute_protos)
set_target_properties(
    compute_protos
    PROPERTIES EXPORT_NAME google-cloud-cpp::compute_protos
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_link_libraries(compute_protos PUBLIC ${deps})

# Some files are too big for the MSVC defaults.
if (MSVC)
    target_compile_options(compute_protos PRIVATE "/bigobj")
endif ()
google_cloud_cpp_install_proto_library_protos(compute_protos
                                              "${PROJECT_SOURCE_DIR}/protos")
google_cloud_cpp_install_proto_library_headers(compute_protos)

# In some configs we need to only generate the protocol definitions from
# `*.proto` files. We achieve this by having this target depend on all proto
# libraries. It has to be defined at the top level of the project.
add_dependencies(google-cloud-cpp-protos compute_protos)
add_dependencies(compute_protos googleapis_download)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_compute_protos-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute_protos"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS compute_protos
    EXPORT google_cloud_cpp_compute_protos-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_compute_protos-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_compute_protos-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute_protos-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute_protos-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute_protos"
    COMPONENT google_cloud_cpp_development)

google_cloud_cpp_add_pkgconfig(
    "compute_protos"
    "The Cloud Compute API C++ Client Library"
    "Provides C++ APIs to use the Cloud Compute API."
    WITH_SHORT_TARGET
    "google_cloud_cpp_cloud_extended_operations_protos"
    "google_cloud_cpp_api_annotations_protos"
    "google_cloud_cpp_api_client_protos"
    "google_cloud_cpp_api_field_behavior_protos")

set(protos_internal_export_globs "${service_dirs}" "${operation_service_dirs}")
list(TRANSFORM protos_internal_export_globs APPEND "*_export.h")
file(
    GLOB relative_protos_internal_export_headers
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${protos_internal_export_globs})
list(SORT relative_protos_internal_export_headers)
set(protos_internal_export_headers)
foreach (file in LISTS relative_protos_internal_export_headers)
    list(APPEND protos_internal_export_headers
         "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${file}>")
endforeach ()
add_library(compute_protos_internal_export INTERFACE)
target_sources(compute_protos_internal_export
               INTERFACE ${protos_internal_export_headers})
target_link_libraries(compute_protos_internal_export
                      INTERFACE google-cloud-cpp::compute_protos)
set_target_properties(
    compute_protos_internal_export
    PROPERTIES EXPORT_NAME google-cloud-cpp::compute_protos_internal_export)

target_include_directories(
    compute_protos_internal_export
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(compute_protos_internal_export
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
google_cloud_cpp_install_headers(compute_protos_internal_export
                                 include/google/cloud/compute)

install(
    EXPORT google_cloud_cpp_compute_protos_internal_export-targets
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute_protos_internal_export"
    COMPONENT google_cloud_cpp_development)
install(
    TARGETS compute_protos_internal_export
    EXPORT google_cloud_cpp_compute_protos_internal_export-targets
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/mocks-config.cmake.in"
    "google_cloud_cpp_compute_protos_internal_export-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_compute_protos_internal_export-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute_protos_internal_export-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_compute_protos_internal_export-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_compute_protos_internal_export"
    COMPONENT google_cloud_cpp_development)

google_cloud_cpp_add_pkgconfig(
    "compute_protos_internal_export"
    "The Cloud Compute API C++ Client Library"
    "Provides C++ header files for stable access to internal Compute protos."
    WITH_SHORT_TARGET
    "google_cloud_cpp_compute_protos"
    "google_cloud_cpp_cloud_extended_operations_protos"
    "google_cloud_cpp_api_annotations_protos"
    "google_cloud_cpp_api_client_protos"
    "google_cloud_cpp_api_field_behavior_protos")
