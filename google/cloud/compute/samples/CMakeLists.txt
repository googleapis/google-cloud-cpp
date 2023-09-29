# ~~~
# Copyright 2023 Google LLC
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

if ((NOT BUILD_TESTING) OR (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS))
    return()
endif ()

set(compute_client_samples # cmake-format: sort
                           disk_samples.cc)

# Export the list of unit tests to a .bzl file so we do not need to maintain the
# list in two places.
export_list_to_bazel("compute_client_samples.bzl" "compute_client_samples" YEAR
                     "2023")

# Generate a target for each integration test.
foreach (fname ${compute_client_samples})
    google_cloud_cpp_add_executable(target "compute" "${fname}")
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    target_link_libraries(${target} PRIVATE google_cloud_cpp_testing
                                            google-cloud-cpp::compute_disks)
    google_cloud_cpp_set_target_name(target "compute" "${fname}")
    set_tests_properties(${target} PROPERTIES LABELS "integration-test")
endforeach ()
