# ~~~
# Copyright 2018 Google LLC
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

set(DOXYGEN_PROJECT_NAME "Google Cloud Firestore C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Firestore")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v${PROJECT_VERSION_MAJOR}"
                       "FIRESTORE_CLIENT_NS=v${PROJECT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/firestore/README.md" "*/google/cloud/firestore/internal/*"
    "*/google/cloud/firestore/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")

include(GoogleCloudCppCommon)

# Enable unit tests
include(CTest)

# Export the version information for Bazel.
include(CreateBazelConfig)

# the client library
add_library(google_cloud_cpp_firestore field_path.cc field_path.h)
target_link_libraries(google_cloud_cpp_firestore
                      PUBLIC google-cloud-cpp::common)
google_cloud_cpp_add_common_options(google_cloud_cpp_firestore)
target_include_directories(
    google_cloud_cpp_firestore
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_firestore
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_firestore
    PROPERTIES EXPORT_NAME "google-cloud-cpp::experimental-firestore"
               VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::experimental-firestore ALIAS
            google_cloud_cpp_firestore)

create_bazel_config(google_cloud_cpp_firestore)

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(firestore_client_unit_tests # cmake-format: sort
                                    field_path_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("firestore_client_unit_tests.bzl"
                         "firestore_client_unit_tests")

    foreach (fname ${firestore_client_unit_tests})
        google_cloud_cpp_add_executable(target "firestore" "${fname}")
        target_link_libraries(
            ${target} PRIVATE google-cloud-cpp::experimental-firestore
                              GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS
    EXPORT firestore-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

# The exports can only be installed if all the dependencies are installed. CMake
# needs to know where the submodules will be installed from,
install(
    EXPORT firestore-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_firestore"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_firestore
    EXPORT firestore-targets
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
    TARGETS google_cloud_cpp_firestore
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
google_cloud_cpp_install_headers(google_cloud_cpp_firestore
                                 include/google/cloud/firestore)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Firestore C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Firestore.")
set(GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_common")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_firestore")

# Install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "google_cloud_cpp_firestore.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_firestore.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_firestore-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_firestore-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_firestore-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_firestore-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_firestore"
    COMPONENT google_cloud_cpp_development)

# TODO(#5726) - generate and install pkg-config and CMake support for backwards
# compatibility
set(GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_firestore")
string(CONCAT GOOGLE_CLOUD_CPP_PC_LIBS "")
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "firestore_client.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)
configure_file("legacy.cmake.in" "firestore_client-config.cmake" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/firestore_client"
    COMPONENT google_cloud_cpp_development)
# TODO(#5726) - END
