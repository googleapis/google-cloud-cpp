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

add_library(bigtable_examples_common bigtable_examples_common.cc
                                     bigtable_examples_common.h)
target_link_libraries(
    bigtable_examples_common
    bigtable_client
    bigtable_protos
    google_cloud_cpp_common
    google_cloud_cpp_grpc_utils
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    bigtable_common_options)

set(bigtable_examples
    # cmake-format: sort
    bigtable_grpc_credentials.cc
    bigtable_hello_app_profile.cc
    bigtable_hello_instance_admin.cc
    bigtable_hello_table_admin.cc
    bigtable_hello_world.cc
    bigtable_instance_admin_snippets.cc
    data_async_snippets.cc
    data_snippets.cc
    instance_admin_async_snippets.cc
    table_admin_async_snippets.cc
    table_admin_iam_policy_snippets.cc
    table_admin_snippets.cc)

set(bigtable_examples_unit_tests # cmake-format: sort
                                 bigtable_examples_common_test.cc)

include(CreateBazelConfig)
export_list_to_bazel("bigtable_examples.bzl" "bigtable_examples")
export_list_to_bazel("bigtable_examples_unit_tests.bzl"
                     "bigtable_examples_unit_tests")
create_bazel_config(bigtable_examples_common)

if (BUILD_TESTING)
    foreach (fname ${bigtable_examples_unit_tests})
        string(REPLACE "/" "_" basename ${fname})
        string(REPLACE ".cc" "" basename ${basename})
        set(target "bigtable_examples_${basename}")
        add_executable(${target} ${fname})
        set_target_properties(${target} PROPERTIES OUTPUT_NAME ${basename})
        target_link_libraries(
            ${target}
            PRIVATE bigtable_examples_common
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
    endforeach ()
endif ()

foreach (fname ${bigtable_examples})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname})
    target_link_libraries(
        ${target}
        PRIVATE bigtable_examples_common
                bigtable_client
                bigtable_protos
                google_cloud_cpp_common
                google_cloud_cpp_grpc_utils
                gRPC::grpc++
                gRPC::grpc
                protobuf::libprotobuf
                bigtable_common_options)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "integration-tests")
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-tests;bigtable-integration-tests")
endforeach ()

# We just know that these tests need to be run against production.
set(bigtable_examples_production
    # cmake-format: sort
    bigtable_grpc_credentials table_admin_iam_policy_snippets)

foreach (target ${bigtable_examples_production})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-tests;integration-tests-no-emulator")
endforeach ()
