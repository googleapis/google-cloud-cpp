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

# Configure sub-project specific Doxygen parameters.
set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(DOXYGEN_PROJECT_NUMBER
    "${STORAGE_CLIENT_VERSION_MAJOR}.${STORAGE_CLIENT_VERSION_MINOR}.${STORAGE_CLIENT_VERSION_PATCH}")
set(DOXYGEN_EXAMPLE_PATH examples)
set(DOXYGEN_PREDEFINED "STORAGE_CLIENT_NS=v${STORAGE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS "*/client/internal/*")

include(GoogleCloudCppCommon)
include(IncludeNlohmannJson)

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

# Search for libcurl, in CMake 3.5 this does not define a target, but it will
# in 3.12 (see https://cmake.org/cmake/help/git-stage/module/FindCURL.html for
# details).  Until then, define the target ourselves if it is missing.
find_package(CURL REQUIRED)
if (NOT TARGET CURL::CURL)
    add_library(CURL::CURL UNKNOWN IMPORTED)
    set_target_properties(CURL::CURL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}")
    set_property(TARGET CURL::CURL APPEND PROPERTY IMPORTED_LOCATION "${CURL_LIBRARY}")
endif ()

# Find OpenSSL and ZLIB, these define targets as-of CMake 3.5 and higher. The
# targets are not needed when linking CURL::CURL as a shared library, only when
# linked statically. Since we only test static linking on Windows you may not
# notice in other platforms.
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Add the library
add_subdirectory(client)
add_subdirectory(tests)

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS storage_common_options nlohmann_json
    EXPORT storage-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT storage-targets DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")
