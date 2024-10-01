# ~~~
# Copyright 2018 Google Inc.
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

# The examples are more readable if we use exceptions for error handling. We had
# to tradeoff readability vs. "making them compile everywhere".
if (NOT GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    return()
endif ()

if (bigtable IN_LIST GOOGLE_CLOUD_CPP_ENABLE AND storage IN_LIST
                                                 GOOGLE_CLOUD_CPP_ENABLE)
    add_executable(gcs2cbt gcs2cbt.cc)
    target_link_libraries(
        gcs2cbt google-cloud-cpp::bigtable google-cloud-cpp::storage
        google-cloud-cpp::grpc_utils)
    google_cloud_cpp_add_common_options(gcs2cbt)
endif ()

find_package(CURL REQUIRED)
include(FindgRPC)

if (spanner IN_LIST GOOGLE_CLOUD_CPP_ENABLE AND iam IN_LIST
                                                GOOGLE_CLOUD_CPP_ENABLE)
    google_cloud_cpp_grpcpp_library(
        hello_world_protos "hello_world_grpc/hello_world.proto"
        PROTO_PATH_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(hello_world_protos
                               PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

    add_executable(grpc_credential_types grpc_credential_types.cc)
    target_link_libraries(
        grpc_credential_types
        PRIVATE hello_world_protos google_cloud_cpp_testing
                google-cloud-cpp::iam google-cloud-cpp::spanner CURL::libcurl)
    google_cloud_cpp_add_common_options(grpc_credential_types)
endif ()

if (batch IN_LIST GOOGLE_CLOUD_CPP_ENABLE AND logging IN_LIST
                                              GOOGLE_CLOUD_CPP_ENABLE)
    add_executable(batch_logging batch_logging.cc)
    target_link_libraries(
        batch_logging PRIVATE google_cloud_cpp_testing google-cloud-cpp::batch
                              google-cloud-cpp::logging)
    google_cloud_cpp_add_common_options(batch_logging)
endif ()

if (apikeys IN_LIST GOOGLE_CLOUD_CPP_ENABLE AND language IN_LIST
                                                GOOGLE_CLOUD_CPP_ENABLE)
    add_executable(api_key api_key.cc)
    target_link_libraries(
        api_key PRIVATE google_cloud_cpp_testing google-cloud-cpp::apikeys
                        google-cloud-cpp::language)
    google_cloud_cpp_add_common_options(api_key)
endif ()

# Label all the binaries as "tests", so they run in our CI builds.
if (NOT BUILD_TESTING)
    return()
endif ()

foreach (test api_key batch_logging gcs2cbt grpc_credential_types)
    if (NOT TARGET "${test}")
        continue()
    endif ()
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(
        ${test} PROPERTIES LABELS
                           "integration-test;integration-test-production")
endforeach ()
