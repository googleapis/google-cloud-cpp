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

# We check this separately.
if (BUILD_TESTING)
    add_library(
        google_cloud_cpp_testing # cmake-format: sort
        assert_ok.cc
        assert_ok.h
        async_sequencer.h
        check_predicate_becomes_false.h
        chrono_literals.h
        command_line_parsing.cc
        command_line_parsing.h
        contains_once.h
        crash_handler.cc
        crash_handler.h
        example_driver.cc
        example_driver.h
        expect_exception.h
        expect_future_error.h
        scoped_environment.cc
        scoped_environment.h
        scoped_log.cc
        scoped_log.h
        scoped_thread.h
        status_matchers.h
        testing_types.cc
        testing_types.h
        timer.cc
        timer.h)
    target_link_libraries(
        google_cloud_cpp_testing
        PUBLIC absl::symbolize absl::failure_signal_handler
               google-cloud-cpp::common GTest::gmock)
    google_cloud_cpp_add_common_options(google_cloud_cpp_testing)

    include(CheckCXXSymbolExists)
    check_cxx_symbol_exists(getrusage sys/resource.h
                            GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE)
    check_cxx_symbol_exists(RUSAGE_THREAD sys/resource.h
                            GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD)

    target_compile_definitions(
        google_cloud_cpp_testing
        PUBLIC
            GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE=$<BOOL:${GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE}>
            GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD=$<BOOL:${GOOGLE_CLOUD_CPP_HAVE_RUSAGE_THREAD}>
    )

    create_bazel_config(google_cloud_cpp_testing YEAR 2019)

    set(google_cloud_cpp_testing_unit_tests
        # cmake-format: sort
        assert_ok_test.cc
        command_line_parsing_test.cc
        contains_once_test.cc
        crash_handler_test.cc
        example_driver_test.cc
        scoped_environment_test.cc
        status_matchers_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_testing_unit_tests.bzl"
                         "google_cloud_cpp_testing_unit_tests" YEAR 2018)

    foreach (fname ${google_cloud_cpp_testing_unit_tests})
        google_cloud_cpp_add_executable(target "common_testing" "${fname}")
        target_link_libraries(
            ${target} PRIVATE google_cloud_cpp_testing google-cloud-cpp::common
                              GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()

    if (NOT GOOGLE_CLOUD_CPP_ENABLE_GRPC)
        return()
    endif ()

    find_package(ProtobufWithTargets REQUIRED)
    add_library(
        google_cloud_cpp_testing_grpc # cmake-format: sort
        fake_completion_queue_impl.cc
        fake_completion_queue_impl.h
        is_proto_equal.cc
        is_proto_equal.h
        mock_async_response_reader.h
        mock_completion_queue_impl.h
        validate_metadata.cc
        validate_metadata.h)
    target_link_libraries(
        google_cloud_cpp_testing_grpc
        PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
               google-cloud-cpp::api_annotations_protos protobuf::libprotobuf
               GTest::gmock)
    google_cloud_cpp_add_common_options(google_cloud_cpp_testing_grpc)

    create_bazel_config(google_cloud_cpp_testing_grpc YEAR 2020)

    set(google_cloud_cpp_testing_grpc_unit_tests # cmake-format: sort
                                                 is_proto_equal_test.cc)

    export_list_to_bazel("google_cloud_cpp_testing_grpc_unit_tests.bzl"
                         "google_cloud_cpp_testing_grpc_unit_tests" YEAR 2020)

    foreach (fname ${google_cloud_cpp_testing_grpc_unit_tests})
        google_cloud_cpp_add_executable(target "common_testing_grpc" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_testing_grpc
                    google_cloud_cpp_testing
                    google-cloud-cpp::common
                    protobuf::libprotobuf
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()
