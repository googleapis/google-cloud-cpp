# ~~~
# Copyright 2022 Google LLC
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

include(GoogleCloudCppCommon)
set(DOXYGEN_PROJECT_NAME "OAuth2 Access Token Generation Library")
set(DOXYGEN_PROJECT_BRIEF
    "Create OAuth2 Access Tokens for any GCP authentication mechanism")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("oauth2" DEPENDS cloud-docs)

add_library(google_cloud_cpp_oauth2 # cmake-format: sort
            access_token_generator.cc access_token_generator.h)
target_link_libraries(
    google_cloud_cpp_oauth2 PUBLIC google-cloud-cpp::rest_internal
                                   google-cloud-cpp::common)
google_cloud_cpp_add_common_options(google_cloud_cpp_oauth2)
target_include_directories(
    google_cloud_cpp_oauth2 PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                   $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_oauth2
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_oauth2
    PROPERTIES EXPORT_NAME "google-cloud-cpp::oauth2"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::oauth2 ALIAS google_cloud_cpp_oauth2)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_oauth2-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_oauth2"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_oauth2
    EXPORT google_cloud_cpp_oauth2-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_oauth2
                                 include/google/cloud/oauth2)

google_cloud_cpp_add_pkgconfig(
    oauth2 "${DOXYGEN_PROJECT_NAME}" "${DOXYGEN_PROJECT_BRIEF}"
    "google_cloud_cpp_rest_internal" "google_cloud_cpp_common")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_oauth2-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_oauth2-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_oauth2-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_oauth2-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_oauth2"
    COMPONENT google_cloud_cpp_development)

create_bazel_config(google_cloud_cpp_oauth2 YEAR 2023)

set(google_cloud_cpp_oauth2_unit_tests # cmake-format: sort
                                       access_token_generator_test.cc)

export_list_to_bazel("google_cloud_cpp_oauth2_unit_tests.bzl"
                     "google_cloud_cpp_oauth2_unit_tests" YEAR 2023)

if (NOT BUILD_TESTING)
    return()
endif ()

foreach (fname IN LISTS google_cloud_cpp_oauth2_unit_tests)
    google_cloud_cpp_add_executable(target "oauth2" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google-cloud-cpp::oauth2
                google_cloud_cpp_testing
                google_cloud_cpp_testing_rest
                google-cloud-cpp::common
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    return()
endif ()

add_executable(oauth2_quickstart "quickstart/quickstart.cc")
target_link_libraries(oauth2_quickstart PRIVATE google-cloud-cpp::oauth2)
google_cloud_cpp_add_common_options(oauth2_quickstart)
add_test(NAME oauth2_quickstart
         COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                 $<TARGET_FILE:oauth2_quickstart>)
set_tests_properties(oauth2_quickstart PROPERTIES LABELS
                                                  "integration-test;quickstart")
