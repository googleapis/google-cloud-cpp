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
    add_library(storage_examples_common # cmake-format: sort
                storage_examples_common.cc storage_examples_common.h)
    target_link_libraries(storage_examples_common storage_client_testing
                          storage_client google_cloud_cpp_common)
    google_cloud_cpp_add_common_options(storage_examples_common)

    set(storage_examples
        # cmake-format: sort
        storage_bucket_acl_samples.cc
        storage_bucket_cors_samples.cc
        storage_bucket_default_kms_key_samples.cc
        storage_bucket_iam_samples.cc
        storage_bucket_requester_pays_samples.cc
        storage_bucket_samples.cc
        storage_default_object_acl_samples.cc
        storage_event_based_hold_samples.cc
        storage_grpc_samples.cc
        storage_lifecycle_management_samples.cc
        storage_notification_samples.cc
        storage_object_acl_samples.cc
        storage_object_cmek_samples.cc
        storage_object_csek_samples.cc
        storage_object_file_transfer_samples.cc
        storage_object_hold_samples.cc
        storage_object_resumable_write_samples.cc
        storage_object_rewrite_samples.cc
        storage_object_samples.cc
        storage_object_versioning_samples.cc
        storage_policy_doc_samples.cc
        storage_public_object_samples.cc
        storage_quickstart.cc
        storage_retention_policy_samples.cc
        storage_service_account_samples.cc
        storage_signed_url_v2_samples.cc
        storage_signed_url_v4_samples.cc
        storage_website_samples.cc)

    set(storage_examples_unit_tests
        # cmake-format: sort
        storage_client_mock_samples.cc storage_examples_common_test.cc)

    include(CreateBazelConfig)
    export_list_to_bazel("storage_examples.bzl" "storage_examples")
    export_list_to_bazel("storage_examples_unit_tests.bzl"
                         "storage_examples_unit_tests")
    create_bazel_config(storage_examples_common)

    # While it is tempting create these targets using a function or a loop, we
    # want to keep this particular file as simple as possible, as it is intended
    # to be part of the examples.

    foreach (fname ${storage_examples_unit_tests})
        google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE storage_examples_common
                    storage_client
                    google_cloud_cpp_testing
                    google_cloud_cpp_common
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        add_test(NAME ${target} COMMAND ${target})
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    foreach (fname ${storage_examples})
        google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE storage_examples_common storage_client_grpc storage_client
                    google_cloud_cpp_common google_cloud_cpp_testing)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    # We just know that these tests need to be run against production.
    set(storage_integration_tests_production
        # cmake-format: sort
        storage_policy_doc_samples.cc storage_signed_url_v2_samples.cc
        storage_signed_url_v4_samples.cc)
    foreach (fname ${storage_integration_tests_production})
        google_cloud_cpp_set_target_name(target "storage_examples" "${fname}")
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-production")
    endforeach ()
endif ()
