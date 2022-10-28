# ~~~
# Copyright 2020 Google LLC
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

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (NOT BUILD_TESTING)
    return()
endif ()

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_generator_golden_protos
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/backup.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/common.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/test.proto
    PROTO_PATH_DIRECTORIES
    ${PROTO_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download)
target_link_libraries(
    google_cloud_cpp_generator_golden_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::api_routing_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)
set_target_properties(google_cloud_cpp_generator_golden_protos
                      PROPERTIES CXX_CLANG_TIDY "")

file(
    GLOB google_cloud_cpp_generator_golden_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "golden/*.h" "golden/*.cc" "golden/internal/*.h" "golden/internal/*.cc"
    "golden/mocks/*.h")
list(SORT google_cloud_cpp_generator_golden_files)

# Create a library from the where the golden code was copied to.
add_library(google_cloud_cpp_generator_golden # cmake-format: sort
            ${google_cloud_cpp_generator_golden_files})

target_include_directories(
    google_cloud_cpp_generator_golden
    PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
           ${PROJECT_BINARY_DIR}/generator/integration_tests)
target_link_libraries(
    google_cloud_cpp_generator_golden
    PUBLIC google_cloud_cpp_generator_golden_protos
           google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::longrunning_operations_protos
           absl::str_format
           protobuf::libprotoc)
google_cloud_cpp_add_common_options(google_cloud_cpp_generator_golden)
target_compile_options(google_cloud_cpp_generator_golden
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# The tests require googletest to be installed. Force CMake to use the config
# file for googletest (that is, the CMake file installed by googletest itself),
# because the generic `FindGTest` module does not define the GTest::gmock
# target, and the target names are also weird.
find_package(GTest CONFIG REQUIRED)

add_library(
    google_cloud_cpp_generator_golden_testing # cmake-format: sort
    tests/mock_golden_kitchen_sink_stub.h tests/mock_golden_thing_admin_stub.h)
target_link_libraries(
    google_cloud_cpp_generator_golden_testing
    PUBLIC google_cloud_cpp_testing google_cloud_cpp_generator_golden
           GTest::gmock_main GTest::gmock GTest::gtest)

set(google_cloud_cpp_generator_golden_tests
    # cmake-format: sort
    tests/golden_kitchen_sink_auth_decorator_test.cc
    tests/golden_kitchen_sink_client_test.cc
    tests/golden_kitchen_sink_connection_test.cc
    tests/golden_kitchen_sink_idempotency_policy_test.cc
    tests/golden_kitchen_sink_logging_decorator_test.cc
    tests/golden_kitchen_sink_metadata_decorator_test.cc
    tests/golden_kitchen_sink_option_defaults_test.cc
    tests/golden_kitchen_sink_round_robin_decorator_test.cc
    tests/golden_kitchen_sink_stub_factory_test.cc
    tests/golden_kitchen_sink_stub_test.cc
    tests/golden_thing_admin_auth_decorator_test.cc
    tests/golden_thing_admin_client_test.cc
    tests/golden_thing_admin_connection_test.cc
    tests/golden_thing_admin_idempotency_policy_test.cc
    tests/golden_thing_admin_logging_decorator_test.cc
    tests/golden_thing_admin_metadata_decorator_test.cc
    tests/golden_thing_admin_option_defaults_test.cc
    tests/golden_thing_admin_round_robin_decorator_test.cc
    tests/golden_thing_admin_stub_factory_test.cc
    tests/golden_thing_admin_stub_test.cc
    tests/plumbing_test.cc)

# Export the list of unit tests to a .bzl file so we do not need to maintain the
# list in two places.
export_list_to_bazel("google_cloud_cpp_generator_golden_tests.bzl"
                     "google_cloud_cpp_generator_golden_tests" YEAR "2020")

foreach (fname ${google_cloud_cpp_generator_golden_tests})
    google_cloud_cpp_add_executable(target "google_cloud_cpp_generator_golden"
                                    "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google_cloud_cpp_generator_golden_testing
                google_cloud_cpp_generator_golden
                google_cloud_cpp_testing
                google_cloud_cpp_testing_grpc
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                protobuf::libprotoc)
    google_cloud_cpp_add_common_options(${target})

    # With googletest it is relatively easy to exceed the default number of
    # sections (~65,000) in a single .obj file. Add the /bigobj option to all
    # the tests, even if it is not needed.
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
endforeach ()
