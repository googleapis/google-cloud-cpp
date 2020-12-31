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

if (BUILD_TESTING)
    add_library(bigtable_examples_common # cmake-format: sort
                bigtable_examples_common.cc bigtable_examples_common.h)
    target_link_libraries(
        bigtable_examples_common
        bigtable_client_testing
        google_cloud_cpp_testing
        google_cloud_cpp_testing_grpc
        google-cloud-cpp::bigtable
        google-cloud-cpp::bigtable_protos
        google-cloud-cpp::common
        google-cloud-cpp::grpc_utils
        gRPC::grpc++
        gRPC::grpc
        protobuf::libprotobuf)
    google_cloud_cpp_add_common_options(bigtable_examples_common)

    set(bigtable_examples
        # cmake-format: sort
        bigtable_grpc_credentials.cc
        bigtable_hello_app_profile.cc
        bigtable_hello_instance_admin.cc
        bigtable_hello_table_admin.cc
        bigtable_hello_world.cc
        bigtable_instance_admin_snippets.cc
        bigtable_table_admin_backup_snippets.cc
        data_async_snippets.cc
        data_filter_snippets.cc
        data_snippets.cc
        read_snippets.cc
        table_admin_iam_policy_snippets.cc
        table_admin_snippets.cc)

    set(bigtable_examples_unit_tests # cmake-format: sort
                                     bigtable_examples_common_test.cc)

    include(CreateBazelConfig)
    export_list_to_bazel("bigtable_examples.bzl" "bigtable_examples")
    export_list_to_bazel("bigtable_examples_unit_tests.bzl"
                         "bigtable_examples_unit_tests")
    create_bazel_config(bigtable_examples_common)

    foreach (fname ${bigtable_examples_unit_tests})
        google_cloud_cpp_add_executable(target "bigtable_examples" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigtable_examples_common
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::bigtable
                    google-cloud-cpp::bigtable_protos
                    google-cloud-cpp::common
                    google-cloud-cpp::grpc_utils
                    absl::time
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
        add_test(NAME ${target} COMMAND ${target})
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    foreach (fname ${bigtable_examples})
        google_cloud_cpp_add_executable(target "bigtable_examples" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigtable_examples_common
                    google_cloud_cpp_testing
                    google-cloud-cpp::bigtable
                    google-cloud-cpp::bigtable_protos
                    google-cloud-cpp::common
                    google-cloud-cpp::grpc_utils
                    absl::time
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    # We just know that these tests need to be run against production.
    set(bigtable_examples_production
        # cmake-format: sort
        bigtable_grpc_credentials.cc bigtable_table_admin_backup_snippets.cc
        table_admin_iam_policy_snippets.cc)

    foreach (fname ${bigtable_examples_production})
        google_cloud_cpp_set_target_name(target "bigtable_examples" "${fname}")
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-production")
    endforeach ()
endif ()
