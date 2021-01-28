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

# TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl release
set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
find_package(absl CONFIG REQUIRED)
unset(FPHSA_NAME_MISMATCHED)

function (pubsub_client_define_benchmarks)
    set(pubsub_client_benchmark_programs # cmake-format: sort
                                         endurance.cc throughput.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("pubsub_client_benchmark_programs.bzl"
                         "pubsub_client_benchmark_programs" YEAR "2020")

    # Generate a target for each benchmark.
    foreach (fname ${pubsub_client_benchmark_programs})
        google_cloud_cpp_add_executable(target "pubsub" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE pubsub_client_testing
                    google_cloud_cpp_testing
                    google-cloud-cpp::pubsub
                    absl::str_format
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})

        add_test(NAME ${target} COMMAND ${target})
        # To automatically smoke-test the benchmarks as part of the CI build we
        # label them as tests.
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
    endforeach ()
endfunction ()

# Only define the benchmarks if testing is enabled. Package maintainers may not
# want to build all the benchmarks every time they create a new package or when
# the package is installed from source.
if (BUILD_TESTING)
    pubsub_client_define_benchmarks()
endif ()
