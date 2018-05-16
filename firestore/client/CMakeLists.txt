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

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# the client library
add_library(firestore_client
    field_path.h
    field_path.cc)
target_link_libraries(firestore_client)
target_include_directories(firestore_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(firestore_client PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(firestore::client ALIAS firestore_client)

include(CreateBazelConfig)
create_bazel_config(firestore_client)

option(FIRESTORE_CLIENT_CLANG_TIDY
    "If set compiles the Cloud Firestore client with clang-tidy."
    "")
if (CLANG_TIDY_EXE AND FIRESTORE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
        firestore_client PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

add_library(firestore_client_testing
    field_path.h
    field_path_test.cc
    )
target_link_libraries(firestore_client_testing
    PUBLIC firestore_client gmock
    PRIVATE firestore_common_options protobuf::libprotobuf)

# List the unit tests, then setup the targets and dependencies.
set(firestore_client_unit_tests
    field_path_test.cc)

# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel("firestore_client_unit_tests.bzl" "firestore_client_unit_tests")

foreach (fname ${firestore_client_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "firestore_client_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
        firestore_client_testing firestore_client gmock firestore_common_options)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

create_bazel_config(firestore_client_testing)

option(FORCE_SANITIZER_ERRORS
        "If set, enable tests that force errors detected by the sanitizers."
        "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(firestore_client_force_sanitizer_failures_test
            PRIVATE -DFIRESTORE_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

option(FORCE_STATIC_ANALYZER_ERRORS
        "If set, enable tests that force errors detected by the static analyzer."
        "")
if (FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(firestore_client_force_sanitizer_failures_test
            PRIVATE -DFIRESTORE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif (FORCE_STATIC_ANALYZER_ERRORS)

install(TARGETS firestore_client
    EXPORT firestore-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/firestore/client
        FILES_MATCHING PATTERN "*.h"
        PATTERN "testing/*" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
    DESTINATION include/firestore/client)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${FIRESTORE_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${FIRESTORE_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${FIRESTORE_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Firestore C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Provides C++ APIs to access Google Cloud Firestore.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lfirestore_client -lfirestore_protos")

# Install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in" "firestore_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Install the CMake configuration files.
configure_file("config.cmake.in" "firestore_client-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
    "firestore_client-config-version.cmake" @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
