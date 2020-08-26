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
    find_package(gRPC)
    add_library(pubsub_samples_common # cmake-format: sort
                pubsub_samples_common.cc pubsub_samples_common.h)
    target_link_libraries(
        pubsub_samples_common
        pubsub_client_testing
        googleapis-c++::pubsub_client
        google_cloud_cpp_common
        google_cloud_cpp_grpc_utils
        gRPC::grpc++
        gRPC::grpc
        protobuf::libprotobuf)
    google_cloud_cpp_add_common_options(pubsub_samples_common)
    create_bazel_config(pubsub_samples_common YEAR 2020)

    set(pubsub_samples_unit_tests # cmake-format: sort
                                  pubsub_samples_common_test.cc)

    foreach (fname ${pubsub_samples_unit_tests})
        google_cloud_cpp_add_executable(target "pubsub_samples" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE pubsub_samples_common
                    googleapis-c++::pubsub_client
                    google_cloud_cpp_testing
                    google_cloud_cpp_common
                    google_cloud_cpp_grpc_utils
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
        add_test(NAME ${target} COMMAND ${target})
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    export_list_to_bazel("pubsub_samples_unit_tests.bzl"
                         "pubsub_samples_unit_tests" YEAR 2020)

    set(pubsub_client_integration_samples # cmake-format: sort
                                          samples.cc)
    set(pubsub_client_unit_samples # cmake-format: sort
                                   mock_publisher.cc mock_subscriber.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("pubsub_client_integration_samples.bzl"
                         "pubsub_client_integration_samples" YEAR "2020")
    export_list_to_bazel("pubsub_client_unit_samples.bzl"
                         "pubsub_client_unit_samples" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${pubsub_client_integration_samples}
                   ${pubsub_client_unit_samples})
        google_cloud_cpp_add_executable(target "pubsub" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE pubsub_samples_common googleapis-c++::pubsub_client
                    google_cloud_cpp_testing GTest::gmock_main GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    foreach (fname ${pubsub_client_integration_samples})
        google_cloud_cpp_set_target_name(target "pubsub" "${fname}")
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    pubsub_client_define_samples()
endif (BUILD_TESTING)
