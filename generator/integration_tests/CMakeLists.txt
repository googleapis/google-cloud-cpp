# ~~~
# Copyright 2020 Google LLC
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

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_generator_testing_grpc_lib
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/backup.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/common.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/test.proto
    PROTO_PATH_DIRECTORIES
    ${PROTO_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download)
target_link_libraries(
    google_cloud_cpp_generator_testing_grpc_lib
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::api_routing_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)
set_target_properties(google_cloud_cpp_generator_testing_grpc_lib
                      PROPERTIES CXX_CLANG_TIDY "")

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    add_subdirectory(golden)
endif (BUILD_TESTING)
