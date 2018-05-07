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
set(STORAGE_CLIENT_VERSION_MAJOR 0)
set(STORAGE_CLIENT_VERSION_MINOR 1)
set(STORAGE_CLIENT_VERSION_PATCH 0)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # gcc-4.8 C++11 support has bugs preventing us from using it.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        message(FATAL_ERROR "GCC version must be at least 4.9!")
    endif()
endif ()

# Configure sub-project specific Doxygen parameters.
set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(DOXYGEN_PROJECT_NUMBER
    "${STORAGE_CLIENT_VERSION_MAJOR}.${STORAGE_CLIENT_VERSION_MINOR}.${STORAGE_CLIENT_VERSION_PATCH}")
set(DOXYGEN_EXAMPLE_PATH examples)
set(DOXYGEN_PREDEFINED "STORAGE_CLIENT_NS=v${STORAGE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS "*/client/internal/*")

include(GoogleCloudCppCommon)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE storage_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(storage_common_options INTERFACE)
google_cloud_cpp_add_common_options(storage_common_options)

# Enable unit tests
enable_testing()

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/../.git)
    execute_process(COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HEAD_LOG ERROR_VARIABLE GIT_HEAD_LOG)
else ()
    set(GIT_HEAD_LOG "(unknown-git-commit)")
endif ()
string(REPLACE "\n" "" GIT_HEAD ${GIT_HEAD_LOG})
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
configure_file(client/build_info.cc.in client/build_info.cc)

# Search for libcurl
find_package(CURL REQUIRED)
if (NOT TARGET CURL::CURL)
    add_library(CURL::CURL UNKNOWN IMPORTED)
    set_target_properties(CURL::CURL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}")
    set_property(TARGET CURL::CURL APPEND PROPERTY IMPORTED_LOCATION "${CURL_LIBRARY}")
endif ()

# Add the library
add_subdirectory(client)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS storage_common_options
    EXPORT storage-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT storage-targets DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")
