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

# This should be included from the top level CMakeLists file
set(FIRESTORE_CLIENT_VERSION_MAJOR 0)
set(FIRESTORE_CLIENT_VERSION_MINOR 0)
set(FIRESTORE_CLIENT_VERSION_PATCH 1)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOXYGEN_PROJECT_NAME "Google Cloud Firestore C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Firestore")
set(
    DOXYGEN_PROJECT_NUMBER
    "${FIRESTORE_CLIENT_VERSION_MAJOR}.${FIRESTORE_CLIENT_VERSION_MINOR}.${FIRESTORE_CLIENT_VERSION_PATCH}"
    )
set(DOXYGEN_PREDEFINED "FIRESTORE_CLIENT_NS=v${FIRESTORE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/firestore/README.md"
    "*/google/cloud/firestore/internal/*"
    "*/google/cloud/firestore/*_test.cc")

include(GoogleCloudCppCommon)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE firestore_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(firestore_common_options INTERFACE)
google_cloud_cpp_add_common_options(firestore_common_options)

# Enable unit tests
enable_testing()

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# the client library
add_library(firestore_client field_path.h field_path.cc)
target_link_libraries(firestore_client PUBLIC google_cloud_cpp_common)
target_include_directories(firestore_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(firestore_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(firestore::client ALIAS firestore_client)

include(CreateBazelConfig)
create_bazel_config(firestore_client)
google_cloud_cpp_add_clang_tidy(firestore_client)

# List the unit tests, then setup the targets and dependencies.
set(firestore_client_unit_tests field_path_test.cc)

# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel("firestore_client_unit_tests.bzl"
                     "firestore_client_unit_tests")

foreach (fname ${firestore_client_unit_tests})
    string(REPLACE "/"
                   "_"
                   target
                   ${fname})
    string(REPLACE ".cc"
                   ""
                   target
                   ${target})
    add_executable(${target} ${fname})
    target_link_libraries(${target}
                          PRIVATE firestore_client
                                  google_cloud_cpp_testing
                                  google_cloud_cpp_common
                                  gmock
                                  firestore_common_options)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS
        EXPORT firestore-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# The exports can only be installed if all the dependencies are installed. CMake
# needs to know where the submodules will be installed from,
install(EXPORT firestore-targets DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)

install(TARGETS firestore_client
        EXPORT firestore-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/google/cloud/firestore
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "testing/*"
        EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h DESTINATION
              include/google/cloud/firestore)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${FIRESTORE_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${FIRESTORE_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${FIRESTORE_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Firestore C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Firestore.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lfirestore_client -lfirestore_protos")

# Install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "firestore_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client.pc" DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Install the CMake configuration files.
configure_file("config.cmake.in" "firestore_client-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
               "firestore_client-config-version.cmake" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config-version.cmake"
          DESTINATION
          ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
