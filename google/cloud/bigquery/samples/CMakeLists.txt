# ~~~
# Copyright 2019 Google LLC
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
    set(bigquery_client_integration_samples # cmake-format: sort
                                            read.cc)
    set(bigquery_client_unit_samples # cmake-format: sort
    )

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_client_integration_samples.bzl"
                         "bigquery_client_integration_samples" YEAR "2019")
    export_list_to_bazel("bigquery_client_unit_samples.bzl"
                         "bigquery_client_unit_samples" YEAR "2019")

    # Generate a target for each unit test.
    foreach (fname ${bigquery_client_integration_samples}
                   ${bigquery_client_unit_samples})
        google_cloud_cpp_add_executable(target "bigquery" "${fname}")
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(${target} PRIVATE googleapis-c++::bigquery_client)
    endforeach ()

    foreach (fname ${bigquery_client_integration_samples})
        google_cloud_cpp_set_target_name(target "bigquery" "${fname}")
        # These tests are intended to be integration tests, but they don't yet
        # work against an emulator or production. Until they work, we'll add the
        # 'integration-test' label so they can be excluded from certain test
        # patterns (e.g., `ctest -LE integration-test`), but we're not adding
        # the 'integration-test-production' or 'integration-test-emulator'
        # labels until they work.
        set_tests_properties(${target} PROPERTIES LABELS "integration-test")
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    bigquery_client_define_samples()
endif (BUILD_TESTING)
