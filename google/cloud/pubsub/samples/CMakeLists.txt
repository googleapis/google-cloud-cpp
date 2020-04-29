# ~~~
# Copyright 2020 Google LLC
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

function (pubsub_client_define_samples)
    set(pubsub_client_integration_samples # cmake-format: sort
                                          samples.cc)
    set(pubsub_client_unit_samples # cmake-format: sort
    )

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("pubsub_client_integration_samples.bzl"
                         "pubsub_client_integration_samples" YEAR "2020")
    export_list_to_bazel("pubsub_client_unit_samples.bzl"
                         "pubsub_client_unit_samples" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${pubsub_client_integration_samples}
                   ${pubsub_client_unit_samples})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        add_executable(${target} ${fname})
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE googleapis-c++::pubsub_client google_cloud_cpp_testing
                    GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        google_cloud_cpp_add_clang_tidy(${target})
    endforeach ()

    foreach (fname ${pubsub_client_integration_samples})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        set_tests_properties(
            ${target}
            PROPERTIES LABELS "integration-tests;integration-tests-no-emulator")
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    pubsub_client_define_samples()
endif (BUILD_TESTING)
