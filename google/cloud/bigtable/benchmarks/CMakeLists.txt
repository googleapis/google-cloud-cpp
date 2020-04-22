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
    bigtable_benchmark_common
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
    bigtable_client
    bigtable_protos
    google_cloud_cpp_common
    google_cloud_cpp_grpc_utils
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    bigtable_common_options)

include(CreateBazelConfig)
create_bazel_config(bigtable_benchmark_common YEAR 2020)

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(bigtable_benchmarks_unit_tests
        bigtable_benchmark_test.cc embedded_server_test.cc
        format_duration_test.cc setup_test.cc)
    export_list_to_bazel("bigtable_benchmarks_unit_tests.bzl"
                         "bigtable_benchmarks_unit_tests" YEAR 2020)

    foreach (fname ${bigtable_benchmarks_unit_tests})
        string(REPLACE "/" "_" basename ${fname})
        string(REPLACE ".cc" "" basename ${basename})
        set(target "bigtable_benchmarks_${basename}")
        add_executable(${target} ${fname})
        set_target_properties(${target} PROPERTIES OUTPUT_NAME ${basename})
        target_link_libraries(
            ${target}
            PRIVATE bigtable_benchmark_common
                    bigtable_client
                    bigtable_client_testing
                    bigtable_protos
                    bigtable_common_options
                    google_cloud_cpp_testing
                    google_cloud_cpp_common
                    google_cloud_cpp_grpc_utils
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
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
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname})
    target_link_libraries(
        ${target}
        PRIVATE bigtable_benchmark_common
                bigtable_client
                bigtable_protos
                bigtable_common_options
                google_cloud_cpp_grpc_utils
                gRPC::grpc++
                gRPC::grpc
                protobuf::libprotobuf)
    if (BUILD_TESTING)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-tests;bigtable-integration-tests")
    endif ()
endforeach ()
