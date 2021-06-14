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

set(google_cloud_cpp_generator_golden_files
    # cmake-format: sort
    golden_kitchen_sink_client.cc
    golden_kitchen_sink_client.h
    golden_kitchen_sink_connection.cc
    golden_kitchen_sink_connection.h
    golden_kitchen_sink_connection_idempotency_policy.cc
    golden_kitchen_sink_connection_idempotency_policy.h
    golden_kitchen_sink_options.h
    golden_thing_admin_client.cc
    golden_thing_admin_client.h
    golden_thing_admin_connection.cc
    golden_thing_admin_connection.h
    golden_thing_admin_connection_idempotency_policy.cc
    golden_thing_admin_connection_idempotency_policy.h
    golden_thing_admin_options.h
    internal/golden_kitchen_sink_logging_decorator.cc
    internal/golden_kitchen_sink_logging_decorator.h
    internal/golden_kitchen_sink_metadata_decorator.cc
    internal/golden_kitchen_sink_metadata_decorator.h
    internal/golden_kitchen_sink_option_defaults.cc
    internal/golden_kitchen_sink_option_defaults.h
    internal/golden_kitchen_sink_stub.cc
    internal/golden_kitchen_sink_stub.h
    internal/golden_kitchen_sink_stub_factory.cc
    internal/golden_kitchen_sink_stub_factory.h
    internal/golden_thing_admin_logging_decorator.cc
    internal/golden_thing_admin_logging_decorator.h
    internal/golden_thing_admin_metadata_decorator.cc
    internal/golden_thing_admin_metadata_decorator.h
    internal/golden_thing_admin_option_defaults.cc
    internal/golden_thing_admin_option_defaults.h
    internal/golden_thing_admin_stub.cc
    internal/golden_thing_admin_stub.h
    internal/golden_thing_admin_stub_factory.cc
    internal/golden_thing_admin_stub_factory.h
    mocks/mock_golden_kitchen_sink_connection.h
    mocks/mock_golden_thing_admin_connection.h
    retry_traits.h
    streaming.cc)

# Export the list of golden files to a .bzl file so we do not need to maintain
# the list in multiple places.
export_list_to_bazel("google_cloud_cpp_generator_golden_files.bzl"
                     "google_cloud_cpp_generator_golden_files" YEAR "2020")

# Create a library from the where the golden code was copied to.
add_library(
    google_cloud_cpp_generator_golden # cmake-format: sort
    golden_kitchen_sink_client.cc
    golden_kitchen_sink_client.h
    golden_kitchen_sink_connection.cc
    golden_kitchen_sink_connection.h
    golden_kitchen_sink_connection_idempotency_policy.cc
    golden_kitchen_sink_connection_idempotency_policy.h
    golden_kitchen_sink_options.h
    golden_thing_admin_client.cc
    golden_thing_admin_client.h
    golden_thing_admin_connection.cc
    golden_thing_admin_connection.h
    golden_thing_admin_connection_idempotency_policy.cc
    golden_thing_admin_connection_idempotency_policy.h
    golden_thing_admin_options.h
    internal/golden_kitchen_sink_auth_decorator.cc
    internal/golden_kitchen_sink_auth_decorator.h
    internal/golden_kitchen_sink_logging_decorator.cc
    internal/golden_kitchen_sink_logging_decorator.h
    internal/golden_kitchen_sink_metadata_decorator.cc
    internal/golden_kitchen_sink_metadata_decorator.h
    internal/golden_kitchen_sink_option_defaults.cc
    internal/golden_kitchen_sink_option_defaults.h
    internal/golden_kitchen_sink_stub.cc
    internal/golden_kitchen_sink_stub.h
    internal/golden_kitchen_sink_stub_factory.cc
    internal/golden_kitchen_sink_stub_factory.h
    internal/golden_thing_admin_auth_decorator.cc
    internal/golden_thing_admin_auth_decorator.h
    internal/golden_thing_admin_logging_decorator.cc
    internal/golden_thing_admin_logging_decorator.h
    internal/golden_thing_admin_metadata_decorator.cc
    internal/golden_thing_admin_metadata_decorator.h
    internal/golden_thing_admin_option_defaults.cc
    internal/golden_thing_admin_option_defaults.h
    internal/golden_thing_admin_stub.cc
    internal/golden_thing_admin_stub.h
    internal/golden_thing_admin_stub_factory.cc
    internal/golden_thing_admin_stub_factory.h
    mocks/mock_golden_kitchen_sink_connection.h
    mocks/mock_golden_kitchen_sink_stub.h
    mocks/mock_golden_thing_admin_connection.h
    mocks/mock_golden_thing_admin_stub.h
    retry_traits.h
    streaming.cc)

target_include_directories(
    google_cloud_cpp_generator_golden
    PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
           ${PROJECT_BINARY_DIR}/generator/integration_tests)
target_link_libraries(
    google_cloud_cpp_generator_golden
    PUBLIC google_cloud_cpp_generator_testing_grpc_lib
           google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::longrunning_operations_protos
           absl::str_format
           protobuf::libprotoc)
google_cloud_cpp_add_common_options(google_cloud_cpp_generator_golden)
target_compile_options(google_cloud_cpp_generator_golden
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

function (google_cloud_cpp_generator_define_golden_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(google_cloud_cpp_generator_golden_tests
        # cmake-format: sort
        tests/golden_kitchen_sink_auth_decorator_test.cc
        tests/golden_kitchen_sink_client_test.cc
        tests/golden_kitchen_sink_connection_test.cc
        tests/golden_kitchen_sink_idempotency_policy_test.cc
        tests/golden_kitchen_sink_logging_decorator_test.cc
        tests/golden_kitchen_sink_metadata_decorator_test.cc
        tests/golden_kitchen_sink_option_defaults_test.cc
        tests/golden_kitchen_sink_stub_factory_test.cc
        tests/golden_kitchen_sink_stub_test.cc
        tests/golden_thing_admin_auth_decorator_test.cc
        tests/golden_thing_admin_client_test.cc
        tests/golden_thing_admin_connection_test.cc
        tests/golden_thing_admin_idempotency_policy_test.cc
        tests/golden_thing_admin_logging_decorator_test.cc
        tests/golden_thing_admin_metadata_decorator_test.cc
        tests/golden_thing_admin_option_defaults_test.cc
        tests/golden_thing_admin_stub_factory_test.cc
        tests/golden_thing_admin_stub_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("google_cloud_cpp_generator_golden_tests.bzl"
                         "google_cloud_cpp_generator_golden_tests" YEAR "2020")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(generator-golden-tests)

    foreach (fname ${google_cloud_cpp_generator_golden_tests})
        google_cloud_cpp_add_executable(
            target "google_cloud_cpp_generator_golden" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_generator_golden
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    protobuf::libprotoc)
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(generator-golden-tests ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_generator_define_golden_tests()
endif (BUILD_TESTING)
