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

find_package(ProtobufWithTargets REQUIRED)
find_package(gRPC REQUIRED)

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)

# TODO(#5854) - enable clang-tidy for the non-generated code!
set(CMAKE_CXX_CLANG_TIDY "")

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_generator_testing_grpc_lib
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/backup.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/common.proto
    ${PROJECT_SOURCE_DIR}/generator/integration_tests/test.proto
    PROTO_PATH_DIRECTORIES
    ${PROTO_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download)
target_link_libraries(
    google_cloud_cpp_generator_testing_grpc_lib
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)

function (google_cloud_cpp_generator_define_integration_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(google_cloud_cpp_generator_integration_tests
        # cmake-format: sort
        generator_integration_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel(
        "google_cloud_cpp_generator_integration_tests.bzl"
        "google_cloud_cpp_generator_integration_tests" YEAR "2020")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(generator-integration-tests)

    # Generate a target for each integration test.
    foreach (fname ${google_cloud_cpp_generator_integration_tests})
        google_cloud_cpp_add_executable(
            target "google_cloud_cpp_generator_integration" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_generator
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
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
        add_dependencies(generator-integration-tests ${target})
    endforeach ()
    if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        return()
    endif ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    add_subdirectory(golden)
    google_cloud_cpp_generator_define_integration_tests()
endif (BUILD_TESTING)
