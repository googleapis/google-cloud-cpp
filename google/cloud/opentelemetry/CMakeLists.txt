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

find_package(opentelemetry-cpp CONFIG REQUIRED)

set(DOXYGEN_PROJECT_NAME "C++ OpenTelemetry Exporters for Google Cloud")
set(DOXYGEN_PROJECT_BRIEF
    "Provides exporters for sending telemetry to Google Cloud services in C++.")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "otel_internal")
set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/quickstart"
                         "${CMAKE_CURRENT_SOURCE_DIR}/samples")
set(GOOGLE_CLOUD_CPP_DOXYGEN_EXTRA_INCLUDES
    "${PROJECT_BINARY_DIR}/google/cloud/monitoring"
    "${PROJECT_BINARY_DIR}/google/cloud/trace")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets(
    "opentelemetry" DEPENDS cloud-docs google-cloud-cpp::monitoring
    google-cloud-cpp::trace)

add_library(
    google_cloud_cpp_opentelemetry # cmake-format: sort
    configure_basic_tracing.cc
    configure_basic_tracing.h
    internal/monitored_resource.cc
    internal/monitored_resource.h
    internal/recordable.cc
    internal/recordable.h
    internal/resource_detector_impl.cc
    internal/resource_detector_impl.h
    internal/time_series.cc
    internal/time_series.h
    monitoring_exporter.cc
    monitoring_exporter.h
    resource_detector.cc
    resource_detector.h
    trace_exporter.cc
    trace_exporter.h)
target_link_libraries(
    google_cloud_cpp_opentelemetry
    PUBLIC google-cloud-cpp::rest_internal google-cloud-cpp::monitoring
           google-cloud-cpp::trace opentelemetry-cpp::metrics
           opentelemetry-cpp::trace)
google_cloud_cpp_add_common_options(google_cloud_cpp_opentelemetry)
target_include_directories(
    google_cloud_cpp_opentelemetry
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_opentelemetry
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_opentelemetry
    PROPERTIES EXPORT_NAME "google-cloud-cpp::opentelemetry"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::opentelemetry ALIAS
            google_cloud_cpp_opentelemetry)
create_bazel_config(google_cloud_cpp_opentelemetry YEAR 2023)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_opentelemetry-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_opentelemetry"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_opentelemetry
    EXPORT google_cloud_cpp_opentelemetry-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_opentelemetry
                                 include/google/cloud/opentelemetry)

google_cloud_cpp_add_pkgconfig(
    opentelemetry
    "OpenTelemetry Exporters for Google Cloud"
    "Provides exporters for sending telemetry to Google Cloud services."
    "google_cloud_cpp_rest_internal"
    "google_cloud_cpp_monitoring"
    "google_cloud_cpp_trace"
    "opentelemetry_metrics"
    "opentelemetry_resources"
    "opentelemetry_trace")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "google_cloud_cpp_opentelemetry-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_opentelemetry-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_opentelemetry-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_opentelemetry-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_opentelemetry"
    COMPONENT google_cloud_cpp_development)

if (NOT BUILD_TESTING)
    return()
endif ()

add_subdirectory(integration_tests)

set(opentelemetry_unit_tests
    # cmake-format: sort
    internal/monitored_resource_test.cc
    internal/recordable_test.cc
    internal/resource_detector_impl_test.cc
    internal/time_series_test.cc
    monitoring_exporter_test.cc
    trace_exporter_test.cc)

export_list_to_bazel("opentelemetry_unit_tests.bzl" "opentelemetry_unit_tests"
                     YEAR "2023")

# Create a custom target so we can say "build all the tests"
add_custom_target(opentelemetry-tests)

# Generate a target for each unit test.
foreach (fname ${opentelemetry_unit_tests})
    google_cloud_cpp_add_executable(target "opentelemetry" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google_cloud_cpp_testing
                google_cloud_cpp_testing_grpc
                google_cloud_cpp_testing_rest
                google-cloud-cpp::opentelemetry
                google_cloud_cpp_monitoring_mocks
                google_cloud_cpp_trace_mocks
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    add_dependencies(opentelemetry-tests ${target})
endforeach ()

# This quickstart depends on the storage library. So only build the quickstart
# runner if storage is enabled.
if (NOT storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    return()
endif ()

include(CTest)
add_executable(opentelemetry_quickstart "quickstart/quickstart.cc")
target_link_libraries(
    opentelemetry_quickstart PRIVATE google-cloud-cpp::opentelemetry
                                     google-cloud-cpp::storage)
google_cloud_cpp_add_common_options(opentelemetry_quickstart)
add_test(
    NAME opentelemetry_quickstart
    COMMAND
        cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
        $<TARGET_FILE:opentelemetry_quickstart>
        GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME GOOGLE_CLOUD_PROJECT)
set_tests_properties(opentelemetry_quickstart
                     PROPERTIES LABELS "integration-test;quickstart")
