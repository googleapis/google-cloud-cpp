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

if (NOT BUILD_TESTING)
    return()
endif ()

add_library(experimental_bigquery_rest_client_benchmarks # cmake-format: sort
            benchmark.cc benchmark.h benchmarks_config.cc benchmarks_config.h)
target_link_libraries(
    experimental_bigquery_rest_client_benchmarks
    PUBLIC google_cloud_cpp_testing
           google-cloud-cpp::experimental-bigquery_rest GTest::gtest)
create_bazel_config(experimental_bigquery_rest_client_benchmarks YEAR "2023")
google_cloud_cpp_add_common_options(
    experimental_bigquery_rest_client_benchmarks)

target_include_directories(
    experimental_bigquery_rest_client_benchmarks
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(experimental_bigquery_rest_client_benchmarks
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# Runs all benchmark unit tests
set(experimental_bigquery_rest_client_benchmark_unit_tests
    # cmake-format: sort
    benchmark_test.cc benchmarks_config_test.cc)

# Export the list of unit tests to a .bzl file so we do not need to maintain the
# list in two places.
export_list_to_bazel(
    "experimental_bigquery_rest_client_benchmark_unit_tests.bzl"
    "experimental_bigquery_rest_client_benchmark_unit_tests" YEAR "2023")

# Generate a target for each benchmark unit test.
foreach (fname ${experimental_bigquery_rest_client_benchmark_unit_tests})
    google_cloud_cpp_add_executable(target "bigquery_v2_minimal" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE experimental_bigquery_rest_client_benchmarks
                bigquery_rest_testing
                google_cloud_cpp_testing
                google-cloud-cpp::experimental-bigquery_rest
                google-cloud-cpp::experimental-bigquery_rest_mocks
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Runs benchmark integration tests.
set(experimental_bigquery_rest_client_benchmark_programs
    # cmake-format: sort
    dataset_benchmark_programs.cc
    job_cancel_benchmark_programs.cc
    job_insert_benchmark_programs.cc
    job_query_benchmark_programs.cc
    job_readonly_benchmark_programs.cc
    project_benchmark_programs.cc
    table_benchmark_programs.cc)

# Export the list of benchmark integration tests to a .bzl file so we do not
# need to maintain the list in two places.
export_list_to_bazel(
    "experimental_bigquery_rest_client_benchmark_programs.bzl"
    "experimental_bigquery_rest_client_benchmark_programs" YEAR "2023")

# Generate a target for each benchmark integration tests.
foreach (fname ${experimental_bigquery_rest_client_benchmark_programs})
    google_cloud_cpp_add_executable(target "bigquery_v2_minimal" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE experimental_bigquery_rest_client_benchmarks
                bigquery_rest_testing google_cloud_cpp_testing
                google-cloud-cpp::experimental-bigquery_rest GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    # To automatically smoke-test the benchmarks as part of the CI build we
    # label them as tests.
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-emulator")
endforeach ()
