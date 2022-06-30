# ~~~
# Copyright 2022 Google LLC
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

add_library(
    google_cloud_cpp_testing_grpc # cmake-format: sort
    fake_completion_queue_impl.cc
    fake_completion_queue_impl.h
    is_proto_equal.cc
    is_proto_equal.h
    mock_async_response_reader.h
    mock_completion_queue_impl.h
    mock_grpc_authentication_strategy.cc
    mock_grpc_authentication_strategy.h
    validate_metadata.cc
    validate_metadata.h)
target_link_libraries(
    google_cloud_cpp_testing_grpc
    PUBLIC google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_routing_protos
           protobuf::libprotobuf
           GTest::gmock)
google_cloud_cpp_add_common_options(google_cloud_cpp_testing_grpc)

create_bazel_config(google_cloud_cpp_testing_grpc YEAR 2020)

set(google_cloud_cpp_testing_grpc_unit_tests # cmake-format: sort
                                             is_proto_equal_test.cc)

export_list_to_bazel("google_cloud_cpp_testing_grpc_unit_tests.bzl"
                     "google_cloud_cpp_testing_grpc_unit_tests" YEAR "2020")

foreach (fname ${google_cloud_cpp_testing_grpc_unit_tests})
    google_cloud_cpp_add_executable(target "common_testing_grpc" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google_cloud_cpp_testing_grpc
                google_cloud_cpp_testing
                google-cloud-cpp::common
                protobuf::libprotobuf
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
endforeach ()
