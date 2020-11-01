# ~~~
# Copyright 2019 Google LLC
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

function (spanner_client_define_benchmarks)
    add_library(spanner_client_benchmarks # cmake-format: sort
                benchmarks_config.cc benchmarks_config.h)
    target_link_libraries(
        spanner_client_benchmarks
        PUBLIC spanner_client_mocks googleapis-c++::spanner_client
               google_cloud_cpp_testing GTest::gmock_main GTest::gmock
               GTest::gtest)
    create_bazel_config(spanner_client_benchmarks YEAR "2019")
    google_cloud_cpp_add_common_options(spanner_client_benchmarks)

    target_include_directories(
        spanner_client_benchmarks
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_compile_options(spanner_client_benchmarks
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    set(spanner_client_benchmark_programs
        # cmake-format: sort
        benchmarks_config_test.cc multiple_rows_cpu_benchmark.cc
        single_row_throughput_benchmark.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_client_benchmark_programs.bzl"
                         "spanner_client_benchmark_programs" YEAR "2019")

    # Generate a target for each benchmark.
    foreach (fname ${spanner_client_benchmark_programs})
        google_cloud_cpp_add_executable(target "spanner" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE spanner_client_benchmarks
                    googleapis-c++::spanner_client
                    spanner_client_testing
                    google_cloud_cpp_testing
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
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
    spanner_client_define_benchmarks()
endif ()
