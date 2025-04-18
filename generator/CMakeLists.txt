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

if (APPLE OR WIN32)
    message(STATUS "The generator directory is only used and tested on Linux")
    return()
endif ()

set(DOXYGEN_EXCLUDE_SYMBOLS "generator_internal")
set(DOXYGEN_EXAMPLE_PATH "")

include(GoogleCloudCppCommon)
find_package(Protobuf CONFIG QUIET)
if (NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif ()
include(IncludeNlohmannJson)
find_package(yaml-cpp CONFIG REQUIRED)

add_library(
    google_cloud_cpp_generator # cmake-format: sort
    generator.cc
    generator.h
    internal/auth_decorator_generator.cc
    internal/auth_decorator_generator.h
    internal/client_generator.cc
    internal/client_generator.h
    internal/codegen_utils.cc
    internal/codegen_utils.h
    internal/connection_generator.cc
    internal/connection_generator.h
    internal/connection_impl_generator.cc
    internal/connection_impl_generator.h
    internal/connection_impl_rest_generator.cc
    internal/connection_impl_rest_generator.h
    internal/connection_rest_generator.cc
    internal/connection_rest_generator.h
    internal/descriptor_utils.cc
    internal/descriptor_utils.h
    internal/discovery_document.h
    internal/discovery_file.cc
    internal/discovery_file.h
    internal/discovery_proto_export_file.cc
    internal/discovery_proto_export_file.h
    internal/discovery_resource.cc
    internal/discovery_resource.h
    internal/discovery_to_proto.cc
    internal/discovery_to_proto.h
    internal/discovery_type_vertex.cc
    internal/discovery_type_vertex.h
    internal/doxygen.cc
    internal/doxygen.h
    internal/format_class_comments.cc
    internal/format_class_comments.h
    internal/format_method_comments.cc
    internal/format_method_comments.h
    internal/forwarding_client_generator.cc
    internal/forwarding_client_generator.h
    internal/forwarding_connection_generator.cc
    internal/forwarding_connection_generator.h
    internal/forwarding_idempotency_policy_generator.cc
    internal/forwarding_idempotency_policy_generator.h
    internal/forwarding_mock_connection_generator.cc
    internal/forwarding_mock_connection_generator.h
    internal/forwarding_options_generator.cc
    internal/forwarding_options_generator.h
    internal/generator_interface.h
    internal/http_annotation_parser.cc
    internal/http_annotation_parser.h
    internal/http_option_utils.cc
    internal/http_option_utils.h
    internal/idempotency_policy_generator.cc
    internal/idempotency_policy_generator.h
    internal/logging_decorator_generator.cc
    internal/logging_decorator_generator.h
    internal/logging_decorator_rest_generator.cc
    internal/logging_decorator_rest_generator.h
    internal/longrunning.cc
    internal/longrunning.h
    internal/make_generators.cc
    internal/make_generators.h
    internal/metadata_decorator_generator.cc
    internal/metadata_decorator_generator.h
    internal/metadata_decorator_rest_generator.cc
    internal/metadata_decorator_rest_generator.h
    internal/mixin_utils.cc
    internal/mixin_utils.h
    internal/mock_connection_generator.cc
    internal/mock_connection_generator.h
    internal/option_defaults_generator.cc
    internal/option_defaults_generator.h
    internal/options_generator.cc
    internal/options_generator.h
    internal/pagination.cc
    internal/pagination.h
    internal/predicate_utils.cc
    internal/predicate_utils.h
    internal/printer.h
    internal/proto_definition_location.h
    internal/request_id.cc
    internal/request_id.h
    internal/resolve_comment_references.cc
    internal/resolve_comment_references.h
    internal/resolve_method_return.cc
    internal/resolve_method_return.h
    internal/retry_traits_generator.cc
    internal/retry_traits_generator.h
    internal/round_robin_decorator_generator.cc
    internal/round_robin_decorator_generator.h
    internal/routing.cc
    internal/routing.h
    internal/sample_generator.cc
    internal/sample_generator.h
    internal/scaffold_generator.cc
    internal/scaffold_generator.h
    internal/service_code_generator.cc
    internal/service_code_generator.h
    internal/sources_generator.cc
    internal/sources_generator.h
    internal/stub_factory_generator.cc
    internal/stub_factory_generator.h
    internal/stub_factory_rest_generator.cc
    internal/stub_factory_rest_generator.h
    internal/stub_generator.cc
    internal/stub_generator.h
    internal/stub_generator_base.cc
    internal/stub_generator_base.h
    internal/stub_rest_generator.cc
    internal/stub_rest_generator.h
    internal/tracing_connection_generator.cc
    internal/tracing_connection_generator.h
    internal/tracing_stub_generator.cc
    internal/tracing_stub_generator.h)
target_include_directories(google_cloud_cpp_generator
                           PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(
    google_cloud_cpp_generator
    PUBLIC google_cloud_cpp_generator_config
           google-cloud-cpp::common
           google-cloud-cpp::rest_internal
           nlohmann_json::nlohmann_json
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_info_protos
           google-cloud-cpp::api_routing_protos
           google-cloud-cpp::cloud_extended_operations_protos
           google-cloud-cpp::longrunning_operations_protos
           absl::str_format
           protobuf::libprotoc
           yaml-cpp::yaml-cpp)
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
add_executable(protoc-gen-cpp_codegen plugin_main.cc)
google_cloud_cpp_add_common_options(protoc-gen-cpp_codegen)
target_link_libraries(
    protoc-gen-cpp_codegen LINK_PUBLIC google_cloud_cpp_generator
    protobuf::libprotoc ${Protobuf_LIBRARIES})

# Generate protobuf code and library
if (COMMAND protobuf_generate)
    set(protobuf_PROTOC_EXE ${Protobuf_PROTOC_EXECUTABLE})
    add_library(google_cloud_cpp_generator_config)
    protobuf_generate(LANGUAGE cpp TARGET google_cloud_cpp_generator_config
                      PROTOS generator_config.proto)
elseif (COMMAND protobuf_generate_cpp)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS generator_config.proto)
    add_library(google_cloud_cpp_generator_config ${PROTO_HDRS} ${PROTO_SRCS})
else ()
    message(FATAL_ERROR "Missing protobuf_generate_cpp and protobuf_generate")
endif ()
target_link_libraries(google_cloud_cpp_generator_config
                      PUBLIC protobuf::libprotobuf)
google_cloud_cpp_add_common_options(google_cloud_cpp_generator_config
                                    NO_WARNINGS)
set_target_properties(google_cloud_cpp_generator_config
                      PROPERTIES CXX_CLANG_TIDY "")

# Build standalone executable
add_executable(google-cloud-cpp-codegen standalone_main.cc)
google_cloud_cpp_add_common_options(google-cloud-cpp-codegen)
target_link_libraries(
    google-cloud-cpp-codegen
    PUBLIC google_cloud_cpp_generator_config google_cloud_cpp_generator
           protobuf::libprotoc absl::flags absl::flags_parse)

# Define the tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_generator_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    add_library(
        google_cloud_cpp_generator_testing # cmake-format: sort
        testing/descriptor_pool_fixture.cc
        testing/descriptor_pool_fixture.h
        testing/error_collectors.cc
        testing/error_collectors.h
        testing/fake_source_tree.cc
        testing/fake_source_tree.h
        testing/printer_mocks.h)
    google_cloud_cpp_add_common_options(google_cloud_cpp_generator_testing)
    target_link_libraries(
        google_cloud_cpp_generator_testing
        PUBLIC google_cloud_cpp_testing GTest::gmock GTest::gtest
               google-cloud-cpp::longrunning_operations_protos
               protobuf::libprotoc)
    create_bazel_config(google_cloud_cpp_generator_testing YEAR "2020")
    target_include_directories(
        google_cloud_cpp_generator_testing PUBLIC ${PROJECT_SOURCE_DIR}
                                                  ${PROJECT_BINARY_DIR})
    target_compile_options(google_cloud_cpp_generator_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    set(google_cloud_cpp_generator_unit_tests
        # cmake-format: sort
        generator_test.cc
        internal/codegen_utils_test.cc
        internal/descriptor_utils_test.cc
        internal/discovery_file_test.cc
        internal/discovery_proto_export_file_test.cc
        internal/discovery_resource_test.cc
        internal/discovery_to_proto_test.cc
        internal/discovery_type_vertex_test.cc
        internal/doxygen_test.cc
        internal/format_class_comments_test.cc
        internal/format_method_comments_test.cc
        internal/http_annotation_parser_test.cc
        internal/http_option_utils_test.cc
        internal/longrunning_test.cc
        internal/make_generators_test.cc
        internal/mixin_utils_test.cc
        internal/pagination_test.cc
        internal/predicate_utils_test.cc
        internal/printer_test.cc
        internal/request_id_test.cc
        internal/resolve_comment_references_test.cc
        internal/resolve_method_return_test.cc
        internal/routing_test.cc
        internal/scaffold_generator_test.cc
        internal/service_code_generator_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("google_cloud_cpp_generator_unit_tests.bzl"
                         "google_cloud_cpp_generator_unit_tests" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${google_cloud_cpp_generator_unit_tests})
        google_cloud_cpp_add_executable(target "generator" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_generator
                    google_cloud_cpp_generator_config
                    google_cloud_cpp_generator_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    GTest::gmock
                    GTest::gtest
                    protobuf::libprotoc)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    add_subdirectory(integration_tests)
    google_cloud_cpp_generator_define_tests()
endif (BUILD_TESTING)
