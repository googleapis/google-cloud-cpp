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

include(GoogleCloudCppCommon)
include(FindProtobufWithTargets)
find_package(absl CONFIG REQUIRED)

find_package(ProtobufWithTargets REQUIRED)

add_library(
    google_cloud_cpp_generator # cmake-format: sort
    generator.cc
    generator.h
    internal/class_generator_interface.h
    internal/codegen_utils.cc
    internal/codegen_utils.h
    internal/descriptor_utils.cc
    internal/descriptor_utils.h
    internal/predicate_utils.cc
    internal/predicate_utils.h
    internal/printer.h
    internal/stub_generator.cc
    internal/stub_generator.h)
target_include_directories(google_cloud_cpp_generator
                           PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(
    google_cloud_cpp_generator
    PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
           googleapis-c++::api_client_protos
           googleapis-c++::longrunning_operations_protos absl::str_format)
google_cloud_cpp_add_common_options(google_cloud_cpp_generator)
target_compile_options(google_cloud_cpp_generator
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(googleapis-c++::google_cloud_cpp_generator ALIAS
            google_cloud_cpp_generator)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_generator YEAR "2020")

# Build protoc plugin executable
add_executable(protoc-gen-cpp_codegen main.cc)
target_link_libraries(protoc-gen-cpp_codegen LINK_PUBLIC
                      google_cloud_cpp_generator protobuf::libprotoc)

# Define the tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_generator_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(google_cloud_cpp_generator_unit_tests
        # cmake-format: sort
        generator_test.cc
        internal/codegen_utils_test.cc
        internal/descriptor_utils_test.cc
        internal/predicate_utils_test.cc
        internal/printer_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("google_cloud_cpp_generator_unit_tests.bzl"
                         "google_cloud_cpp_generator_unit_tests" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${google_cloud_cpp_generator_unit_tests})
        google_cloud_cpp_add_executable(target "google_cloud_cpp_generator"
                                        "${fname}")
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
    endforeach ()
endfunction ()

add_subdirectory(integration_tests)

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_generator_define_tests()
endif (BUILD_TESTING)
