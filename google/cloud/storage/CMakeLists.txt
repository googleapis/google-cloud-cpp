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
set(STORAGE_CLIENT_VERSION_MAJOR 0)
set(STORAGE_CLIENT_VERSION_MINOR 1)
set(STORAGE_CLIENT_VERSION_PATCH 0)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(
    DOXYGEN_PROJECT_NUMBER
    "${STORAGE_CLIENT_VERSION_MAJOR}.${STORAGE_CLIENT_VERSION_MINOR}.${STORAGE_CLIENT_VERSION_PATCH}"
    )
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(DOXYGEN_PREDEFINED "STORAGE_CLIENT_NS=v${STORAGE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/storage/README.md"
    "*/google/cloud/storage/ci/*"
    "*/google/cloud/storage/examples/*.md"
    "*/google/cloud/storage/internal/*"
    "*/google/cloud/storage/testing/*"
    "*/google/cloud/storage/tests/*"
    "*/google/cloud/storage/*_test.cc")

include(GoogleCloudCppCommon)
include(IncludeNlohmannJson)

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE storage_common_options), which implicitly
# adds the options to those targets, but does *not* add the options to any
# targets that depend on them.  This is a CMake idiom to add options to targets
# without having to painfully set special variables, it also allows users to add
# more common options without having to touch the code.
add_library(storage_common_options INTERFACE)
google_cloud_cpp_add_common_options(storage_common_options)

# Enable unit tests
enable_testing()

# Search for libcurl, in CMake 3.5 this does not define a target, but it will in
# 3.12 (see https://cmake.org/cmake/help/git-stage/module/FindCURL.html for
# details).  Until then, define the target ourselves if it is missing.
find_package(CURL REQUIRED)
if (NOT TARGET CURL::CURL)
    add_library(CURL::CURL UNKNOWN IMPORTED)
    set_property(TARGET CURL::CURL
                 APPEND
                 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}")
    set_property(TARGET CURL::CURL
                 APPEND
                 PROPERTY IMPORTED_LOCATION "${CURL_LIBRARY}")
endif ()
# If the library is static, we need to explicitly link its dependencies.
# However, we should not do so for shared libraries, because the version of
# OpenSSL (for example) found by find_package() may be newer than the version
# linked against libcurl.
if ("${CURL_LIBRARY}" MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    set_property(TARGET CURL::CURL
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          OpenSSL::SSL
                          OpenSSL::Crypto
                          ZLIB::ZLIB)
    message(STATUS "CURL linkage will be static")
else()
    message(STATUS "CURL linkage will be non-static")
endif ()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# the client library
add_library(storage_client
            ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
            bucket_access_control.h
            bucket_access_control.cc
            bucket_metadata.h
            bucket_metadata.cc
            client.h
            client.cc
            client_options.h
            client_options.cc
            credentials.h
            credentials.cc
            internal/access_control_common.h
            internal/access_control_common.cc
            internal/authorized_user_credentials.h
            internal/binary_data_as_debug_string.h
            internal/binary_data_as_debug_string.cc
            internal/raw_client_wrapper_utils.h
            internal/common_metadata.h
            internal/credential_constants.h
            internal/curl_handle.h
            internal/curl_handle.cc
            internal/curl_download_request.h
            internal/curl_download_request.cc
            internal/curl_request.h
            internal/curl_request.cc
            internal/curl_request_builder.h
            internal/curl_request_builder.cc
            internal/curl_upload_request.cc
            internal/curl_upload_request.h
            internal/curl_wrappers.h
            internal/curl_wrappers.cc
            internal/curl_client.h
            internal/curl_client.cc
            internal/curl_streambuf.h
            internal/curl_streambuf.cc
            internal/delete_object_request.h
            internal/delete_object_request.cc
            internal/empty_response.h
            internal/empty_response.cc
            internal/generic_object_request.h
            internal/generic_request.h
            internal/get_bucket_metadata_request.h
            internal/get_bucket_metadata_request.cc
            internal/get_object_metadata_request.h
            internal/get_object_metadata_request.cc
            internal/google_application_default_credentials_file.h
            internal/google_application_default_credentials_file.cc
            internal/http_response.h
            internal/insert_object_media_request.h
            internal/insert_object_media_request.cc
            internal/list_buckets_request.h
            internal/list_buckets_request.cc
            internal/list_object_acl_request.h
            internal/list_object_acl_request.cc
            internal/list_objects_request.h
            internal/list_objects_request.cc
            internal/logging_client.h
            internal/logging_client.cc
            internal/metadata_parser.h
            internal/metadata_parser.cc
            internal/nljson.h
            internal/openssl_util.h
            internal/object_acl_requests.h
            internal/object_acl_requests.cc
            internal/object_streambuf.h
            internal/object_streambuf.cc
            internal/parse_rfc3339.h
            internal/parse_rfc3339.cc
            internal/patch_builder.h
            internal/raw_client.h
            internal/read_object_range_request.h
            internal/read_object_range_request.cc
            internal/retry_client.h
            internal/retry_client.cc
            internal/service_account_credentials.h
            list_buckets_reader.h
            list_buckets_reader.cc
            list_objects_reader.h
            list_objects_reader.cc
            object_access_control.h
            object_access_control.cc
            object_metadata.h
            object_metadata.cc
            object_stream.h
            object_stream.cc
            retry_policy.h
            status.h
            storage_class.h
            version.h
            version.cc
            well_known_headers.h
            well_known_parameters.h)
target_link_libraries(storage_client
                      PUBLIC google_cloud_cpp_common
                             nlohmann_json
                             CURL::CURL
                             Threads::Threads
                             OpenSSL::SSL
                             OpenSSL::Crypto
                             ZLIB::ZLIB
                      PRIVATE storage_common_options)
if (WIN32)
    target_link_libraries(storage_client PUBLIC wsock32 ws2_32)
endif ()
target_include_directories(storage_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(storage_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(storage_client_testing
            testing/canonical_errors.h
            testing/mock_client.h
            testing/mock_http_request.h
            testing/mock_http_request.cc
            testing/retry_tests.h)
target_link_libraries(storage_client_testing
                      PUBLIC storage_client nlohmann_json gmock
                      PRIVATE storage_common_options)
target_include_directories(storage_client_testing
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(storage_client_testing
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CreateBazelConfig)
create_bazel_config(storage_client)
google_cloud_cpp_add_clang_tidy(storage_client)
create_bazel_config(storage_client_testing)

# List the unit tests, then setup the targets and dependencies.
set(storage_client_unit_tests
    bucket_access_control_test.cc
    bucket_metadata_test.cc
    bucket_test.cc
    client_object_acl_test.cc
    client_test.cc
    client_write_object_test.cc
    credentials_test.cc
    internal/access_control_common_test.cc
    internal/authorized_user_credentials_test.cc
    internal/binary_data_as_debug_string_test.cc
    internal/delete_object_request_test.cc
    internal/get_bucket_metadata_request_test.cc
    internal/get_object_metadata_request_test.cc
    internal/google_application_default_credentials_file_test.cc
    internal/insert_object_media_request_test.cc
    internal/list_buckets_request_test.cc
    internal/list_object_acl_request_test.cc
    internal/list_objects_request_test.cc
    internal/logging_client_test.cc
    internal/metadata_parser_test.cc
    internal/nljson_test.cc
    internal/object_acl_requests_test.cc
    internal/parse_rfc3339_test.cc
    internal/patch_builder_test.cc
    internal/retry_client_test.cc
    internal/read_object_range_request_test.cc
    internal/service_account_credentials_test.cc
    list_buckets_reader_test.cc
    list_objects_reader_test.cc
    object_access_control_test.cc
    object_metadata_test.cc
    object_test.cc
    retry_policy_test.cc
    storage_class_test.cc
    storage_client_options_test.cc
    link_test.cc)

foreach (fname ${storage_client_unit_tests})
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
                          PRIVATE storage_client_testing
                                  google_cloud_cpp_testing
                                  storage_client
                                  google_cloud_cpp_testing
                                  gmock
                                  CURL::CURL
                                  storage_common_options
                                  nlohmann_json)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel(
    "storage_client_unit_tests.bzl" "storage_client_unit_tests")

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
install(EXPORT storage-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")

install(TARGETS storage_client
        EXPORT storage-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/storage/client
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "testing/*"
        EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h DESTINATION
              include/storage/client)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${STORAGE_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${STORAGE_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${STORAGE_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Storage C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Storage.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lstorage_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "storage_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client.pc" DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "storage_client-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
               "storage_client-config-version.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config-version.cmake"
              DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")
