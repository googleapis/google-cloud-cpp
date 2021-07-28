# ~~~
# Copyright 2021 Google LLC
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

function (bigquery_client_define_samples)
    find_package(gRPC)
    set(bigquery_client_samples # cmake-format: sort
                                bigquery_read_samples.cc)
    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_client_samples.bzl"
                         "bigquery_client_samples" YEAR "2021")

    # Generate a target for each unit test.
    foreach (fname ${bigquery_client_samples})
        google_cloud_cpp_add_executable(target "bigquery" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_testing google-cloud-cpp::bigquery
                    GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    foreach (fname ${bigquery_client_samples})
        google_cloud_cpp_set_target_name(target "bigquery" "${fname}")
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
    endforeach ()

    set(bigquery_client_mock_samples # cmake-format: sort
                                     mock_bigquery_read.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_client_mock_samples.bzl"
                         "bigquery_client_mock_samples" YEAR "2021")

    # Generate a target for each unit test.
    foreach (fname ${bigquery_client_mock_samples})
        google_cloud_cpp_add_executable(target "bigquery" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_testing google-cloud-cpp::bigquery
                    GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    foreach (fname ${bigquery_client_mock_samples})
        google_cloud_cpp_set_target_name(target "bigquery" "${fname}")
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    bigquery_client_define_samples()
endif (BUILD_TESTING)
