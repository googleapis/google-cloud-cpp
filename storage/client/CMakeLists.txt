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
add_library(storage_client
    build_info.h
    ${CMAKE_CURRENT_BINARY_DIR}/build_info.cc
    ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
    version.h
    version.cc)
target_link_libraries(storage_client PUBLIC CURL::CURL Threads::Threads
    PRIVATE storage_common_options)
target_include_directories(storage_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(storage_client PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

option(STORAGE_CLIENT_CLANG_TIDY
    "If set compiles the Google Cloud Storage client with clang-tidy."
    "")
if (CLANG_TIDY_EXE AND STORAGE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
        storage_client PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

# List the unit tests, then setup the targets and dependencies.
set(storage_client_unit_tests
    link_test.cc)

foreach (fname ${storage_client_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "storage_client_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
        storage_client gmock CURL::CURL storage_common_options)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

install(TARGETS storage_client
    EXPORT storage-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/storage/client
    FILES_MATCHING PATTERN "*.h"
    PATTERN "testing/*" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
    DESTINATION include/storage/client)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${STORAGE_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${STORAGE_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${STORAGE_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Storage C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Provides C++ APIs to access Google Cloud Storage.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lstorage_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in" "storage_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "storage_client-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
    "storage_client-config-version.cmake" @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")
