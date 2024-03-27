# ~~~
# Copyright 2018 Google LLC
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

if (NOT BUILD_TESTING)
    return()
endif ()

add_library(storage_examples_common # cmake-format: sort
            storage_examples_common.cc storage_examples_common.h)
target_link_libraries(storage_examples_common storage_client_testing
                      google-cloud-cpp::storage google-cloud-cpp::common)
google_cloud_cpp_add_common_options(storage_examples_common)

# We just know that these tests need to be run against production.
set(storage_examples_production
    # cmake-format: sort
    storage_policy_doc_samples.cc storage_signed_url_v2_samples.cc
    storage_signed_url_v4_samples.cc)

set(storage_examples
    # cmake-format: sort
    storage_bucket_acl_samples.cc
    storage_bucket_autoclass_samples.cc
    storage_bucket_cors_samples.cc
    storage_bucket_default_kms_key_samples.cc
    storage_bucket_iam_samples.cc
    storage_bucket_requester_pays_samples.cc
    storage_bucket_samples.cc
    storage_bucket_soft_delete_samples.cc
    storage_client_initialization_samples.cc
    storage_client_per_operation_samples.cc
    storage_default_object_acl_samples.cc
    storage_event_based_hold_samples.cc
    storage_lifecycle_management_samples.cc
    storage_notification_samples.cc
    storage_object_acl_samples.cc
    storage_object_cmek_samples.cc
    storage_object_csek_samples.cc
    storage_object_file_transfer_samples.cc
    storage_object_hold_samples.cc
    storage_object_preconditions_samples.cc
    storage_object_resumable_write_samples.cc
    storage_object_rewrite_samples.cc
    storage_object_samples.cc
    storage_object_versioning_samples.cc
    storage_public_object_samples.cc
    storage_quickstart.cc
    storage_retention_policy_samples.cc
    storage_service_account_samples.cc
    storage_website_samples.cc)
list(APPEND storage_examples ${storage_examples_production})
list(SORT storage_examples)

set(storage_examples_unit_tests
    # cmake-format: sort
    storage_client_mock_samples.cc storage_examples_common_test.cc)

include(CreateBazelConfig)
export_list_to_bazel("storage_examples.bzl" "storage_examples" YEAR "2018")
export_list_to_bazel("storage_examples_unit_tests.bzl"
                     "storage_examples_unit_tests" YEAR "2018")
create_bazel_config(storage_examples_common)

foreach (fname ${storage_examples_unit_tests})
    google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_examples_common
                google_cloud_cpp_testing
                google-cloud-cpp::storage
                google-cloud-cpp::common
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    add_test(NAME ${target} COMMAND ${target})
    google_cloud_cpp_add_common_options(${target})
endforeach ()

foreach (fname ${storage_examples})
    google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
    target_link_libraries(
        ${target} PRIVATE storage_examples_common google_cloud_cpp_testing
                          google-cloud-cpp::storage google-cloud-cpp::common)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-emulator")
    google_cloud_cpp_add_common_options(${target})
endforeach ()

foreach (fname ${storage_examples_production})
    google_cloud_cpp_set_target_name(target "storage_examples" "${fname}")
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-production")
endforeach ()

# These lists are always defined and exported to Bazel. They are only *used* if
# storage_grpc is defined.
set(storage_grpc_examples # cmake-format: sort
                          storage_async_samples.cc storage_grpc_samples.cc)
# The mocking examples take the form of a unit test.
set(storage_grpc_unit_tests # cmake-format: sort
                            storage_async_mock_samples.cc)

export_list_to_bazel("storage_grpc_examples.bzl" "storage_grpc_examples"
                     "storage_grpc_unit_tests" YEAR "2023")

if (NOT "storage_grpc" IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    return()
endif ()

foreach (fname ${storage_grpc_unit_tests})
    google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_examples_common
                google-cloud-cpp::storage_grpc
                google-cloud-cpp::storage_grpc_mocks
                google-cloud-cpp::storage
                google-cloud-cpp::common
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    add_test(NAME ${target} COMMAND ${target})
    google_cloud_cpp_add_common_options(${target})
endforeach ()

foreach (fname ${storage_grpc_examples})
    google_cloud_cpp_add_executable(target "storage_examples" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_examples_common google-cloud-cpp::storage_grpc
                google-cloud-cpp::storage google-cloud-cpp::common)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-emulator")
    google_cloud_cpp_add_common_options(${target})
endforeach ()

if (NOT opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    return()
endif ()

# This is too custom for the loop.
google_cloud_cpp_add_executable(target "storage_examples"
                                "storage_otel_samples.cc")
target_link_libraries(
    ${target}
    PRIVATE storage_examples_common google-cloud-cpp::opentelemetry
            google-cloud-cpp::storage_grpc google-cloud-cpp::storage
            google-cloud-cpp::common)
add_test(NAME ${target} COMMAND ${target})
set_tests_properties(${target} PROPERTIES LABELS "integration-test")
google_cloud_cpp_add_common_options(${target})
