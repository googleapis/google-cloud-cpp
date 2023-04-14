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
set(DOXYGEN_EXAMPLE_PATH "")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("opentelemetry_sdk" DEPENDS cloud-docs
                                 google-cloud-cpp::trace)

add_library(
    google_cloud_cpp_opentelemetry_sdk # cmake-format: sort
    internal/recordable.cc internal/recordable.h trace_exporter.cc
    trace_exporter.h)
target_link_libraries(google_cloud_cpp_opentelemetry_sdk
                      PUBLIC google-cloud-cpp::trace opentelemetry-cpp::trace)
google_cloud_cpp_add_common_options(google_cloud_cpp_opentelemetry_sdk)
target_include_directories(
    google_cloud_cpp_opentelemetry_sdk
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_opentelemetry_sdk
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_opentelemetry_sdk
    PROPERTIES EXPORT_NAME "google-cloud-cpp::experimental-opentelemetry_sdk"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::experimental-opentelemetry_sdk ALIAS
            google_cloud_cpp_opentelemetry_sdk)
create_bazel_config(google_cloud_cpp_opentelemetry_sdk YEAR 2023)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_opentelemetry_sdk-targets
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_opentelemetry_sdk"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_opentelemetry_sdk
    EXPORT google_cloud_cpp_opentelemetry_sdk-targets
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
    TARGETS google_cloud_cpp_opentelemetry_sdk
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_opentelemetry_sdk
                                 include/google/cloud/opentelemetry)

google_cloud_cpp_add_pkgconfig(
    opentelemetry_sdk "OpenTelemetry Exporters for Google Cloud"
    "Provides exporters for sending telemetry to Google Cloud services."
    "google_cloud_cpp_trace")

# Create and install the CMake configuration files.
configure_file("config.cmake.in"
               "google_cloud_cpp_opentelemetry_sdk-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_opentelemetry_sdk-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_opentelemetry_sdk-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_opentelemetry_sdk-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_opentelemetry_sdk"
    COMPONENT google_cloud_cpp_development)

if (NOT BUILD_TESTING)
    return()
endif ()

set(opentelemetry_sdk_unit_tests
    # cmake-format: sort
    internal/recordable_test.cc trace_exporter_test.cc)

export_list_to_bazel("opentelemetry_sdk_unit_tests.bzl"
                     "opentelemetry_sdk_unit_tests" YEAR "2023")

# Create a custom target so we can say "build all the tests"
add_custom_target(opentelemetry_sdk-tests)

# Generate a target for each unit test.
foreach (fname ${opentelemetry_sdk_unit_tests})
    google_cloud_cpp_add_executable(target "opentelemetry" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google_cloud_cpp_testing
                google_cloud_cpp_testing_grpc
                google-cloud-cpp::experimental-opentelemetry_sdk
                google_cloud_cpp_trace_mocks
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})

    # With googletest it is relatively easy to exceed the default number of
    # sections (~65,000) in a single .obj file. Add the /bigobj option to all
    # the tests, even if it is not needed.
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
    add_dependencies(opentelemetry_sdk-tests ${target})
endforeach ()
