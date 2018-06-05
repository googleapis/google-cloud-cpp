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

# This should be included from the top level CMakeLists file
set(GOOGLE_CLOUD_CPP_VERSION_MAJOR 0)
set(GOOGLE_CLOUD_CPP_VERSION_MINOR 1)
set(GOOGLE_CLOUD_CPP_VERSION_PATCH 0)
# Generate the version information from the CMake values.
configure_file(internal/version_info.h.in internal/version_info.h)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GoogleCloudCppCommon)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE bigtable_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(google_cloud_cpp_common_options INTERFACE)
google_cloud_cpp_add_common_options(google_cloud_cpp_common_options)

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
    execute_process(COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HEAD_LOG ERROR_VARIABLE GIT_HEAD_LOG)
else ()
    set(GIT_HEAD_LOG "(unknown-git-commit)")
endif ()
string(REPLACE "\n" "" GIT_HEAD ${GIT_HEAD_LOG})
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
configure_file(internal/build_info.cc.in internal/build_info.cc)

# the client library
add_library(google_cloud_cpp_common
    internal/build_info.h
    ${CMAKE_CURRENT_BINARY_DIR}/internal/build_info.cc
    internal/port_platform.h
    internal/setenv.h
    internal/setenv.cc
    internal/throw_delegate.h
    internal/throw_delegate.cc
    log.h
    log.cc
    version.h)
target_link_libraries(google_cloud_cpp_common PUBLIC Threads::Threads
    PRIVATE google_cloud_cpp_common_options)
target_include_directories(google_cloud_cpp_common PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_common PUBLIC
    ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_common)
google_cloud_cpp_add_clang_tidy(google_cloud_cpp_common)

set(google_cloud_cpp_common_unit_tests
    internal/throw_delegate_test.cc
    log_test.cc)

# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel("google_cloud_cpp_common_unit_tests.bzl"
    "google_cloud_cpp_common_unit_tests")

foreach (fname ${google_cloud_cpp_common_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "google_cloud_cpp_common_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
        google_cloud_cpp_common gmock google_cloud_cpp_common_options)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT google_cloud_cpp_common-targets DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS google_cloud_cpp_common google_cloud_cpp_common_options
    EXPORT google_cloud_cpp_common-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/google/cloud
    FILES_MATCHING PATTERN "*.h")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/internal/version_info.h
    DESTINATION include/google/cloud/internal)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${GOOGLE_CLOUD_CPP_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${GOOGLE_CLOUD_CPP_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "Google Cloud C++ Client Library Common Components")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Common Components used by the Google Cloud C++ Client Libraries.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_common")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in" "google_cloud_cpp_common.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "google_cloud_cpp_common-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
        "google_cloud_cpp_common-config-version.cmake" @ONLY)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common")

add_subdirectory(bigtable)
add_subdirectory(firestore)
add_subdirectory(storage)
