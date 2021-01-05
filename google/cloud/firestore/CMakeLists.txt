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
add_library(firestore_client field_path.cc field_path.h)
target_link_libraries(firestore_client)
google_cloud_cpp_add_common_options(firestore_client)
target_include_directories(
    firestore_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(firestore_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    firestore_client PROPERTIES VERSION ${PROJECT_VERSION}
                                SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(firestore::client ALIAS firestore_client)

create_bazel_config(firestore_client)

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
            ${target} PRIVATE firestore_client GTest::gmock_main GTest::gmock
                              GTest::gtest)
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
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# The exports can only be installed if all the dependencies are installed. CMake
# needs to know where the submodules will be installed from,
install(EXPORT firestore-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/firestore_client")

install(
    TARGETS firestore_client
    EXPORT firestore-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
google_cloud_cpp_install_headers(firestore_client
                                 include/google/cloud/firestore)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Firestore C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Firestore.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lfirestore_client -lfirestore_protos")

# Install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "firestore_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "firestore_client-config.cmake" @ONLY)
write_basic_package_version_file(
    "firestore_client-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/firestore_client")
