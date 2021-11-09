# ~~~
# Copyright 2021 Google Inc.
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
set(bigtable_admin_integration_tests
    # cmake-format: sort
    admin_backup_integration_test.cc admin_iam_policy_integration_test.cc
    instance_admin_integration_test.cc table_admin_integration_test.cc)

include(CreateBazelConfig)
export_list_to_bazel("bigtable_admin_integration_tests.bzl"
                     "bigtable_admin_integration_tests" YEAR 2021)

foreach (fname ${bigtable_admin_integration_tests})
    google_cloud_cpp_add_executable(target "bigtable_admin" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE bigtable_client_testing
                google_cloud_cpp_testing
                google_cloud_cpp_testing_grpc
                google-cloud-cpp::bigtable
                google-cloud-cpp::bigtable_protos
                google-cloud-cpp::common
                google-cloud-cpp::grpc_utils
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
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
set(bigtable_integration_tests_production
    # cmake-format: sort
    admin_backup_integration_test.cc admin_iam_policy_integration_test.cc)

foreach (fname ${bigtable_integration_tests_production})
    google_cloud_cpp_set_target_name(target "bigtable_admin" "${fname}")
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-production")
endforeach ()
