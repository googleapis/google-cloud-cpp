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

if (BUILD_TESTING)
    # TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl
    # release
    set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
    find_package(absl CONFIG REQUIRED)
    unset(FPHSA_NAME_MISMATCHED)

    add_library(storage_benchmarks # cmake-format: sort
                benchmark_utils.cc benchmark_utils.h bounded_queue.h)
    target_link_libraries(storage_benchmarks PUBLIC storage_client)
    google_cloud_cpp_add_common_options(storage_benchmarks)

    include(CheckCXXSymbolExists)
    check_cxx_symbol_exists(getrusage sys/resource.h
                            GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE)
    check_cxx_symbol_exists(RUSAGE_THREAD sys/resource.h
                            GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD)
    target_compile_definitions(
        storage_benchmarks
        PUBLIC
            GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE=$<BOOL:${GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE}>
            GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD=$<BOOL:${GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD}>
    )

    include(CreateBazelConfig)
    create_bazel_config(storage_benchmarks YEAR 2019)

    set(storage_benchmark_programs
        # cmake-format: sort
        ${storage_benchmark_programs_manual_run}
        storage_file_transfer_benchmark.cc
        storage_parallel_uploads_benchmark.cc
        storage_shard_throughput_benchmark.cc
        storage_throughput_vs_cpu_benchmark.cc)

    foreach (fname ${storage_benchmark_programs})
        google_cloud_cpp_add_executable(target "storage" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE storage_benchmarks
                    storage_client
                    storage_client_testing
                    google_cloud_cpp_common
                    absl::strings
                    CURL::libcurl
                    Threads::Threads
                    nlohmann_json)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
    endforeach ()

    export_list_to_bazel("storage_benchmark_programs.bzl"
                         "storage_benchmark_programs" YEAR 2019)

    # List the unit tests, then setup the targets and dependencies.
    set(storage_benchmarks_unit_tests
        # cmake-format: sort
        benchmark_make_random_test.cc benchmark_parse_args_test.cc
        benchmark_parser_test.cc benchmark_utils_test.cc)

    foreach (fname ${storage_benchmarks_unit_tests})
        google_cloud_cpp_add_executable(target "storage_benchmarks" "${fname}")
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        if (BUILD_TESTING)
            target_link_libraries(
                ${target}
                PRIVATE storage_benchmarks
                        storage_client_testing
                        google_cloud_cpp_testing
                        storage_client
                        GTest::gmock_main
                        GTest::gmock
                        GTest::gtest
                        CURL::libcurl
                        absl::strings
                        nlohmann_json)
            google_cloud_cpp_add_common_options(${target})
            if (MSVC)
                target_compile_options(${target} PRIVATE "/bigobj")
            endif ()
            add_test(NAME ${target} COMMAND ${target})
        endif ()
    endforeach ()
    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storage_benchmarks_unit_tests.bzl"
                         "storage_benchmarks_unit_tests" YEAR 2019)
endif ()
