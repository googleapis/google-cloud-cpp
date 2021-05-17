# ~~~
# Copyright 2018 Google Inc.
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

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

add_executable(gcs2cbt gcs2cbt.cc)
target_link_libraries(gcs2cbt google-cloud-cpp::bigtable
                      google-cloud-cpp::storage google-cloud-cpp::grpc_utils)
google_cloud_cpp_add_common_options(gcs2cbt)

if (BUILD_TESTING)
    include(FindGMockWithTargets)
    include(FindCurlWithTargets)
    include(FindgRPC)

    google_cloud_cpp_grpcpp_library(
        hello_world_protos "hello_world_grpc/hello_world.proto"
        PROTO_PATH_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(hello_world_protos
                               PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

    add_executable(grpc_credential_types grpc_credential_types.cc)
    target_link_libraries(
        grpc_credential_types
        PRIVATE hello_world_protos google_cloud_cpp_testing
                google-cloud-cpp::experimental-iam google-cloud-cpp::spanner
                CURL::libcurl)
    google_cloud_cpp_add_common_options(grpc_credential_types)

    foreach (test gcs2cbt grpc_credential_types)
        add_test(NAME ${test} COMMAND ${test})
        set_tests_properties(
            ${test} PROPERTIES LABELS
                               "integration-test;integration-test-production")
    endforeach ()
endif ()
