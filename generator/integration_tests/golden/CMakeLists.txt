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
    connection_options.gcpcxx.pb.cc
    connection_options.gcpcxx.pb.h
    internal/database_admin_logging_decorator.gcpcxx.pb.cc
    internal/database_admin_logging_decorator.gcpcxx.pb.h
    internal/database_admin_metadata_decorator.gcpcxx.pb.cc
    internal/database_admin_metadata_decorator.gcpcxx.pb.h
    internal/database_admin_stub.gcpcxx.pb.cc
    internal/database_admin_stub.gcpcxx.pb.h
    internal/database_admin_stub_factory.gcpcxx.pb.cc
    internal/database_admin_stub_factory.gcpcxx.pb.h)

# Export the list of golden files to a .bzl file so we do not need to maintain
# the list in multiple places.
export_list_to_bazel("google_cloud_cpp_generator_golden_files.bzl"
                     "google_cloud_cpp_generator_golden_files" YEAR "2020")

# Create a library from the where the golden code was copied to.
add_library(
    google_cloud_cpp_generator_golden_lib # cmake-format: sort
    connection_options.gcpcxx.pb.cc
    connection_options.gcpcxx.pb.h
    internal/database_admin_logging_decorator.gcpcxx.pb.cc
    internal/database_admin_logging_decorator.gcpcxx.pb.h
    internal/database_admin_metadata_decorator.gcpcxx.pb.cc
    internal/database_admin_metadata_decorator.gcpcxx.pb.h
    internal/database_admin_stub.gcpcxx.pb.cc
    internal/database_admin_stub.gcpcxx.pb.h
    internal/database_admin_stub_factory.gcpcxx.pb.cc
    internal/database_admin_stub_factory.gcpcxx.pb.h)

target_include_directories(
    google_cloud_cpp_generator_golden_lib
    PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
           ${PROJECT_BINARY_DIR}/generator/integration_tests)
target_link_libraries(
    google_cloud_cpp_generator_golden_lib
    PUBLIC google_cloud_cpp_generator_testing_grpc_lib
           google_cloud_cpp_grpc_utils
           google_cloud_cpp_common
           googleapis-c++::api_client_protos
           googleapis-c++::longrunning_operations_protos
           absl::str_format
           protobuf::libprotoc)
google_cloud_cpp_add_common_options(google_cloud_cpp_generator_golden_lib)
target_compile_options(google_cloud_cpp_generator_golden_lib
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

function (google_cloud_cpp_generator_define_golden_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(google_cloud_cpp_generator_golden_tests
        # cmake-format: sort
        golden_connection_options_test.cc
        golden_logging_decorator_test.cc
        golden_metadata_decorator_test.cc
        golden_stub_factory_test.cc
        golden_stub_test.cc)

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
            PRIVATE google_cloud_cpp_generator_golden_lib
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
