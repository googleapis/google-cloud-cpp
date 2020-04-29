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

# The integration tests, these are simply programs that connect to the Cloud
# Bigtable emulator.
set(bigtable_client_integration_tests
    # cmake-format: sort
    admin_async_future_integration_test.cc
    admin_iam_policy_integration_test.cc
    admin_integration_test.cc
    data_async_future_integration_test.cc
    data_integration_test.cc
    filters_integration_test.cc
    instance_admin_async_future_integration_test.cc
    instance_admin_integration_test.cc
    mutations_integration_test.cc)

include(CreateBazelConfig)
export_list_to_bazel("bigtable_client_integration_tests.bzl"
                     "bigtable_client_integration_tests")

foreach (fname ${bigtable_client_integration_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname})
    target_link_libraries(
        ${target}
        PRIVATE bigtable_client_testing
                bigtable_client
                bigtable_protos
                google_cloud_cpp_testing
                google_cloud_cpp_common
                google_cloud_cpp_grpc_utils
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                gRPC::grpc++
                gRPC::grpc
                protobuf::libprotobuf
                bigtable_common_options)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-tests;bigtable-integration-tests")
endforeach ()

add_executable(instance_admin_emulator instance_admin_emulator.cc)
target_link_libraries(
    instance_admin_emulator
    PRIVATE bigtable_client
            bigtable_protos
            google_cloud_cpp_grpc_utils
            gRPC::grpc++
            gRPC::grpc
            protobuf::libprotobuf
            bigtable_common_options)

# We just know that these tests need to be run against production.
set(bigtable_integration_tests_production # cmake-format: sort
                                          admin_iam_policy_integration_test)

foreach (target ${bigtable_integration_tests_production})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-tests;integration-tests-no-emulator")
endforeach ()
