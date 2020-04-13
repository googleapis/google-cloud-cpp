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

function (google_cloud_cpp_test_name_to_target var fname)
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set("${var}"
        "${target}"
        PARENT_SCOPE)
endfunction ()

function (bigquery_client_define_samples)
    set(bigquery_client_integration_samples # cmake-format: sort
                                            read.cc)
    set(bigquery_client_unit_samples # cmake-format: sort
    )

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_client_integration_samples.bzl"
                         "bigquery_client_integration_samples")
    export_list_to_bazel("bigquery_client_unit_samples.bzl"
                         "bigquery_client_unit_samples")

    # Generate a target for each unit test.
    foreach (fname ${bigquery_client_integration_samples}
                   ${bigquery_client_unit_samples})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        add_executable(${target} ${fname})
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(${target} PRIVATE googleapis-c++::bigquery_client)
    endforeach ()

    foreach (fname ${bigquery_client_integration_samples})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        set_tests_properties(${target} PROPERTIES LABELS "integration-tests")
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    bigquery_client_define_samples()
endif (BUILD_TESTING)
