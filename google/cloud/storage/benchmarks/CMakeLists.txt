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

if (NOT BUILD_TESTING OR NOT GOOGLE_CLOUD_CPP_ENABLE_GRPC)
    return()
endif ()

add_library(
    storage_benchmarks # cmake-format: sort
    aggregate_download_throughput_options.cc
    aggregate_download_throughput_options.h
    aggregate_upload_throughput_options.cc
    aggregate_upload_throughput_options.h
    benchmark_utils.cc
    benchmark_utils.h
    bounded_queue.h
    create_dataset_options.cc
    create_dataset_options.h
    throughput_experiment.cc
    throughput_experiment.h
    throughput_options.cc
    throughput_options.h
    throughput_result.cc
    throughput_result.h)
target_link_libraries(
    storage_benchmarks
    PUBLIC storage_client_testing
           google_cloud_cpp_testing
           google-cloud-cpp::storage_grpc
           google-cloud-cpp::storage
           google-cloud-cpp::grpc_utils
           google-cloud-cpp::storage_protos
           CURL::libcurl)
google_cloud_cpp_add_common_options(storage_benchmarks)
if (Protobuf_VERSION VERSION_LESS "23")
    target_compile_definitions(
        storage_benchmarks
        PRIVATE GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
endif ()

include(CreateBazelConfig)
create_bazel_config(storage_benchmarks YEAR 2019)

set(storage_benchmark_programs
    # cmake-format: sort
    aggregate_download_throughput_benchmark.cc
    aggregate_upload_throughput_benchmark.cc
    create_dataset.cc
    storage_file_transfer_benchmark.cc
    storage_parallel_uploads_benchmark.cc
    storage_throughput_vs_cpu_benchmark.cc
    throughput_experiment_test.cc)

foreach (fname ${storage_benchmark_programs})
    google_cloud_cpp_add_executable(target "storage" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_benchmarks
                storage_client_testing
                google_cloud_cpp_testing
                google-cloud-cpp::storage
                google-cloud-cpp::common
                absl::strings
                Threads::Threads
                nlohmann_json)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-emulator")
endforeach ()

export_list_to_bazel("storage_benchmark_programs.bzl"
                     "storage_benchmark_programs" YEAR "2019")

# List the unit tests, then setup the targets and dependencies.
set(storage_benchmarks_unit_tests
    # cmake-format: sort
    aggregate_download_throughput_options_test.cc
    aggregate_upload_throughput_options_test.cc
    benchmark_make_random_test.cc
    benchmark_parser_test.cc
    create_dataset_options_test.cc
    throughput_options_test.cc
    throughput_result_test.cc)

foreach (fname ${storage_benchmarks_unit_tests})
    google_cloud_cpp_add_executable(target "storage_benchmarks" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_benchmarks
                storage_client_testing
                google_cloud_cpp_testing
                google-cloud-cpp::storage
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                absl::strings
                nlohmann_json)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
endforeach ()
# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel("storage_benchmarks_unit_tests.bzl"
                     "storage_benchmarks_unit_tests" YEAR "2019")

# We reduce the effort required to implement benchmarks for asynchronous APIs by
# using C++20 coroutines, exceptions, and newer compilers.  The rest of this
# file contains only these benchmarks, so return early if the features are not
# available.
if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    return()
endif ()
if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND (CMAKE_CXX_COMPILER_VERSION
                                                VERSION_LESS 10))
    return()
endif ()

add_executable(async_throughput_benchmark async_throughput_benchmark.cc)
target_compile_features(async_throughput_benchmark PRIVATE cxx_std_20)
# Skip clang-tidy because it will make recommendations based on C++20 and/or
# C++17. Some of these recommendations will be about header files in
# `google/cloud/*`. We cannot apply those recommendations because the headers
# are also used with C++14.
set_target_properties(async_throughput_benchmark PROPERTIES CXX_CLANG_TIDY "")
target_link_libraries(async_throughput_benchmark
                      PRIVATE storage_benchmarks google-cloud-cpp::storage_grpc)
