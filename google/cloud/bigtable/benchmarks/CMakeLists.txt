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

add_library(
    bigtable_benchmark_common # cmake-format: sort
    benchmark.cc
    benchmark.h
    constants.h
    embedded_server.cc
    embedded_server.h
    random_mutation.cc
    random_mutation.h
    setup.cc
    setup.h)
target_link_libraries(
    bigtable_benchmark_common
    google-cloud-cpp::bigtable
    google-cloud-cpp::bigtable_protos
    google-cloud-cpp::common
    google-cloud-cpp::grpc_utils
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf)
google_cloud_cpp_add_common_options(bigtable_benchmark_common)

include(CreateBazelConfig)
create_bazel_config(bigtable_benchmark_common YEAR 2020)

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(bigtable_benchmarks_unit_tests
        # cmake-format: sort
        bigtable_benchmark_test.cc embedded_server_test.cc
        format_duration_test.cc random_mutation_test.cc setup_test.cc)
    export_list_to_bazel("bigtable_benchmarks_unit_tests.bzl"
                         "bigtable_benchmarks_unit_tests" YEAR 2020)

    foreach (fname ${bigtable_benchmarks_unit_tests})
        google_cloud_cpp_add_executable(target "bigtable_benchmarks" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigtable_benchmark_common
                    bigtable_client_testing
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
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

set(bigtable_benchmark_programs
    # cmake-format: sort
    apply_read_latency_benchmark.cc endurance_benchmark.cc
    read_sync_vs_async_benchmark.cc scan_throughput_benchmark.cc)
export_list_to_bazel("bigtable_benchmark_programs.bzl"
                     "bigtable_benchmark_programs")

foreach (fname ${bigtable_benchmark_programs})
    google_cloud_cpp_add_executable(target "bigtable" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE bigtable_benchmark_common
                google-cloud-cpp::bigtable
                google-cloud-cpp::bigtable_protos
                google-cloud-cpp::grpc_utils
                gRPC::grpc++
                gRPC::grpc
                protobuf::libprotobuf)
    google_cloud_cpp_add_common_options(${target})
    if (BUILD_TESTING)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
    endif ()
endforeach ()
