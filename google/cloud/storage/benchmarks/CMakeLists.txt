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

add_library(storage_benchmarks benchmark_utils.h benchmark_utils.cc)
target_link_libraries(storage_benchmarks
                      PUBLIC storage_client
                      PRIVATE storage_common_options
                              google_cloud_cpp_common_options)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(
    getrusage sys/resource.h GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE)
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
    storage_file_transfer_benchmark.cc
    storage_latency_benchmark.cc
    storage_throughput_benchmark.cc
    storage_throughput_vs_cpu_benchmark.cc)

foreach (fname ${storage_benchmark_programs})
    string(REPLACE "/"
                   "_"
                   target
                   ${fname})
    string(REPLACE ".cc"
                   ""
                   target
                   ${target})
    add_executable(${target} ${fname})
    target_link_libraries(${target}
                          PRIVATE storage_benchmarks
                                  storage_client
                                  google_cloud_cpp_common
                                  CURL::libcurl
                                  Threads::Threads
                                  nlohmann_json
                                  storage_common_options
                                  google_cloud_cpp_common_options)
endforeach ()

export_list_to_bazel("storage_benchmark_programs.bzl"
                     "storage_benchmark_programs"
                     YEAR
                     2019)

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(storage_benchmarks_unit_tests
        benchmark_parser_test.cc
        benchmark_make_random_test.cc
        benchmark_parse_args_test.cc
        benchmark_utils_test.cc)

    foreach (fname ${storage_benchmarks_unit_tests})
        string(REPLACE "/"
                       "_"
                       target
                       ${fname})
        string(REPLACE ".cc"
                       ""
                       target
                       ${target})
        add_executable(${target} ${fname})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        if (BUILD_TESTING)
            target_link_libraries(${target}
                                  PRIVATE storage_benchmarks
                                          storage_client_testing
                                          google_cloud_cpp_testing
                                          storage_client
                                          GTest::gmock_main
                                          GTest::gmock
                                          GTest::gtest
                                          CURL::libcurl
                                          storage_common_options
                                          nlohmann_json)
            if (MSVC)
                target_compile_options(${target} PRIVATE "/bigobj")
            endif ()
            add_test(NAME ${target} COMMAND ${target})
        endif ()
    endforeach ()
    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storage_benchmarks_unit_tests.bzl"
                         "storage_benchmarks_unit_tests"
                         YEAR
                         2019)
endif ()
