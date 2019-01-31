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

# This should be included from the top level CMakeLists file
set(BIGTABLE_CLIENT_VERSION_MAJOR 0)
set(BIGTABLE_CLIENT_VERSION_MINOR 6)
set(BIGTABLE_CLIENT_VERSION_PATCH 0)

set(DOXYGEN_PROJECT_NAME "Google Cloud Bigtable C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Bigtable")
set(
    DOXYGEN_PROJECT_NUMBER
    "${BIGTABLE_CLIENT_VERSION_MAJOR}.${BIGTABLE_CLIENT_VERSION_MINOR}.${BIGTABLE_CLIENT_VERSION_PATCH}"
    )
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(DOXYGEN_PREDEFINED "BIGTABLE_CLIENT_NS=v${BIGTABLE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/bigtable/README.md"
    "*/google/cloud/bigtable/benchmarks/*"
    "*/google/cloud/bigtable/ci/*"
    "*/google/cloud/bigtable/examples/*.md"
    "*/google/cloud/bigtable/internal/*"
    "*/google/cloud/bigtable/testing/*"
    "*/google/cloud/bigtable/tests/*"
    "*/google/cloud/bigtable/tools/*"
    "*/google/cloud/bigtable/*_test.cc")
set(DOXYGEN_TAGFILES "${PROJECT_BINARY_DIR}/google/cloud/cloud.tag=../common")

include(GoogleCloudCppCommon)
if (TARGET bigtable-docs AND TARGET cloud-docs)
    add_dependencies(bigtable-docs cloud-docs)
endif ()

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE bigtable_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(bigtable_common_options INTERFACE)
google_cloud_cpp_add_common_options(bigtable_common_options)

# Configure the location of proto files, particulary the googleapis protos.
list(APPEND PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis"
            "${PROJECT_SOURCE_DIR}")

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

# Include the functions to compile proto files.
include(CompileProtos)
protobuf_generate_cpp(
    PROTO_SOURCES
    PROTO_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/common.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/instance.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/table.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/data.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/iam/v1/iam_policy.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/iam/v1/policy.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/status.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/error_details.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/annotations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/auth.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/http.proto)
grpc_generate_cpp(
    GRPCPP_SOURCES
    GRPCPP_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto)

# Create a library with the generated files from the relevant protos.
add_library(bigtable_protos
            ${PROTO_SOURCES}
            ${PROTO_HDRS}
            ${GRPCPP_SOURCES}
            ${GRPCPP_HDRS})
target_link_libraries(bigtable_protos
                      gRPC::grpc++
                      gRPC::grpc
                      protobuf::libprotobuf
                      bigtable_common_options)
target_include_directories(bigtable_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_protos
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    bigtable_protos
    PROPERTIES
        VERSION
        ${BIGTABLE_CLIENT_VERSION_MAJOR}.${BIGTABLE_CLIENT_VERSION_MINOR}.${BIGTABLE_CLIENT_VERSION_PATCH}
        SOVERSION
        ${BIGTABLE_CLIENT_VERSION_MAJOR})
add_library(bigtable::protos ALIAS bigtable_protos)

# Enable unit tests
include(CTest)

add_dependencies(google-cloud-cpp-dependencies bigtable_protos)

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)
export_variables_to_bazel("bigtable_client_version.bzl"
                          BIGTABLE_CLIENT_VERSION_MAJOR
                          BIGTABLE_CLIENT_VERSION_MINOR
                          BIGTABLE_CLIENT_VERSION_PATCH)

# the client library
add_library(bigtable_client
            admin_client.h
            admin_client.cc
            app_profile_config.h
            app_profile_config.cc
            async_cluster_list_responses.h
            async_instance_list_responses.h
            async_operation.h
            bigtable_strong_types.h
            ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
            cell.h
            client_options.h
            client_options.cc
            cluster_config.h
            cluster_config.cc
            column_family.h
            completion_queue.h
            completion_queue.cc
            data_client.h
            data_client.cc
            filters.h
            grpc_error.h
            grpc_error.cc
            instance_admin_client.h
            instance_admin_client.cc
            instance_admin.h
            instance_admin.cc
            instance_config.h
            instance_config.cc
            instance_update_config.h
            instance_update_config.cc
            internal/async_bulk_apply.h
            internal/async_check_consistency.h
            internal/async_future_from_callback.h
            internal/async_list_app_profiles.h
            internal/async_list_clusters.h
            internal/async_list_instances.h
            internal/async_longrunning_op.h
            internal/async_loop_op.h
            internal/async_op_traits.h
            internal/async_poll_op.h
            internal/async_sample_row_keys.h
            internal/async_sample_row_keys.cc
            internal/async_read_row_operation.h
            internal/async_retry_multi_page.h
            internal/async_retry_op.h
            internal/async_retry_unary_rpc.h
            internal/async_retry_unary_rpc_and_poll.h
            internal/async_row_reader.h
            internal/bulk_mutator.h
            internal/bulk_mutator.cc
            internal/completion_queue_impl.h
            internal/completion_queue_impl.cc
            internal/common_client.h
            internal/common_client.cc
            internal/conjunction.h
            internal/encoder.h
            internal/endian.h
            internal/endian.cc
            internal/grpc_error_delegate.h
            internal/grpc_error_delegate.cc
            internal/instance_admin.h
            internal/instance_admin.cc
            internal/poll_longrunning_operation.h
            internal/prefix_range_end.h
            internal/prefix_range_end.cc
            internal/readrowsparser.h
            internal/readrowsparser.cc
            internal/rpc_policy_parameters.inc
            internal/rpc_policy_parameters.h
            internal/rowreaderiterator.h
            internal/rowreaderiterator.cc
            internal/strong_type.h
            internal/table.h
            internal/table.cc
            internal/table_admin.h
            internal/table_admin.cc
            internal/unary_client_utils.h
            idempotent_mutation_policy.h
            idempotent_mutation_policy.cc
            mutations.h
            mutations.cc
            polling_policy.h
            polling_policy.cc
            read_modify_write_rule.h
            row.h
            row_key_sample.h
            row_range.h
            row_range.cc
            row_reader.h
            row_reader.cc
            row_set.h
            row_set.cc
            rpc_backoff_policy.h
            rpc_backoff_policy.cc
            rpc_retry_policy.h
            rpc_retry_policy.cc
            metadata_update_policy.h
            metadata_update_policy.cc
            table.h
            table.cc
            table_admin.h
            table_admin.cc
            table_config.h
            table_config.cc
            table_strong_types.h
            version.h
            version.cc)
target_link_libraries(bigtable_client
                      PUBLIC bigtable_protos
                             google_cloud_cpp_common
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE bigtable_common_options)
target_include_directories(bigtable_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    bigtable_client
    PROPERTIES
        VERSION
        ${BIGTABLE_CLIENT_VERSION_MAJOR}.${BIGTABLE_CLIENT_VERSION_MINOR}.${BIGTABLE_CLIENT_VERSION_PATCH}
        SOVERSION
        ${BIGTABLE_CLIENT_VERSION_MAJOR})
add_library(bigtable::client ALIAS bigtable_client)

include(CreateBazelConfig)
create_bazel_config(bigtable_client)
google_cloud_cpp_add_clang_tidy(bigtable_client)

if (BUILD_TESTING)
    add_library(bigtable_client_testing
                testing/embedded_server_test_fixture.h
                testing/embedded_server_test_fixture.cc
                testing/internal_table_test_fixture.h
                testing/internal_table_test_fixture.cc
                testing/mock_admin_client.h
                testing/mock_data_client.h
                testing/mock_instance_admin_client.h
                testing/inprocess_data_client.h
                testing/inprocess_data_client.cc
                testing/inprocess_admin_client.h
                testing/inprocess_admin_client.cc
                testing/mock_completion_queue.h
                testing/mock_mutate_rows_reader.h
                testing/mock_read_rows_reader.h
                testing/mock_response_reader.h
                testing/mock_sample_row_keys_reader.h
                testing/table_integration_test.h
                testing/table_integration_test.cc
                testing/table_test_fixture.h
                testing/table_test_fixture.cc)
    target_link_libraries(bigtable_client_testing
                          PUBLIC bigtable_client
                                 bigtable_protos
                                 GTest::gmock_main
                                 GTest::gmock
                                 GTest::gtest
                                 gRPC::grpc++
                                 gRPC::grpc
                                 protobuf::libprotobuf
                          PRIVATE bigtable_common_options)

    create_bazel_config(bigtable_client_testing)

    # List the unit tests, then setup the targets and dependencies.
    set(bigtable_client_unit_tests
        admin_client_test.cc
        app_profile_config_test.cc
        bigtable_version_test.cc
        cell_test.cc
        client_options_test.cc
        cluster_config_test.cc
        column_family_test.cc
        completion_queue_test.cc
        data_client_test.cc
        filters_test.cc
        force_sanitizer_failures_test.cc
        grpc_error_test.cc
        idempotent_mutation_policy_test.cc
        instance_admin_client_test.cc
        instance_admin_test.cc
        instance_config_test.cc
        instance_update_config_test.cc
        internal/async_check_consistency_test.cc
        internal/async_future_from_callback_test.cc
        internal/async_list_app_profiles_test.cc
        internal/async_list_clusters_test.cc
        internal/async_list_instances_test.cc
        internal/async_longrunning_op_test.cc
        internal/async_poll_op_test.cc
        internal/async_retry_multi_page_test.cc
        internal/async_retry_op_test.cc
        internal/async_retry_unary_rpc_and_poll_test.cc
        internal/bulk_mutator_test.cc
        internal/table_async_check_and_mutate_row_test.cc
        internal/instance_admin_test.cc
        internal/grpc_error_delegate_test.cc
        internal/prefix_range_end_test.cc
        internal/table_admin_test.cc
        internal/table_async_apply_test.cc
        internal/table_async_bulk_apply_test.cc
        internal/table_async_row_reader_test.cc
        internal/table_async_sample_row_keys_test.cc
        internal/table_test.cc
        mutations_test.cc
        table_admin_test.cc
        table_apply_test.cc
        table_bulk_apply_test.cc
        table_check_and_mutate_row_test.cc
        table_config_test.cc
        table_readrow_test.cc
        table_readrows_test.cc
        table_sample_row_keys_test.cc
        table_test.cc
        table_readmodifywriterow_test.cc
        read_modify_write_rule_test.cc
        row_reader_test.cc
        row_test.cc
        row_range_test.cc
        row_set_test.cc
        rpc_backoff_policy_test.cc
        metadata_update_policy_test.cc
        rpc_retry_policy_test.cc
        polling_policy_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("bigtable_client_unit_tests.bzl"
                         "bigtable_client_unit_tests")

    # Append this unit test after exporting to Bazel because it requires special
    # treatment
    list(APPEND bigtable_client_unit_tests internal/readrowsparser_test.cc)

    foreach (fname ${bigtable_client_unit_tests})
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
                              PRIVATE bigtable_client_testing
                                      bigtable_client
                                      bigtable_protos
                                      google_cloud_cpp_testing
                                      google_cloud_cpp_common
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest
                                      gRPC::grpc++
                                      gRPC::grpc
                                      protobuf::libprotobuf
                                      bigtable_common_options)
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

option(FORCE_SANITIZER_ERRORS
       "If set, enable tests that force errors detected by the sanitizers." "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(bigtable_client_force_sanitizer_failures_test
                               PRIVATE -DBIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

option(FORCE_STATIC_ANALYZER_ERRORS
       "If set, enable tests that force errors detected by the static analyzer."
       "")
if (FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(bigtable_client_force_sanitizer_failures_test
                               PRIVATE
                               -DBIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif (FORCE_STATIC_ANALYZER_ERRORS)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(benchmarks)
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "external"
    OR "${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "external")
    set(GOOGLE_CLOUD_CPP_BIGTABLE_ENABLE_INSTALL "OFF")
    message(
        WARNING
            "The install target has been disabled for the bigtable_client"
            " library because one of the dependencies is set to use external"
            " projects. When building against external projects the resulting"
            " libraries are not installable. The necessary dependencies may"
            " not installed in your system, or the versions installed in your"
            " system may not match the version used to compile the"
            " google-cloud-cpp libraries. "
            ""
            "Please consult the README.md file for details on how to compile "
            "against pre-installed versions of the google-cloud-cpp "
            "dependencies.
GPRC     PROVIDER = ${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}
PROTOBUF PROVIDER = ${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}")
else()
    set(GOOGLE_CLOUD_CPP_BIGTABLE_ENABLE_INSTALL "ON")
endif ()

if ("${GOOGLE_CLOUD_CPP_BIGTABLE_ENABLE_INSTALL}")
    # Install the libraries and headers in the locations determined by
    # GNUInstallDirs
    install(TARGETS bigtable_protos bigtable_common_options
            EXPORT bigtable-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Install proto generated files into include/google.
    google_cloud_cpp_install_headers(bigtable_protos include)

    # Export the CMake targets to make it easy to create configuration files.
    install(EXPORT bigtable-targets
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bigtable_client")

    install(TARGETS bigtable_client
            EXPORT bigtable-targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    google_cloud_cpp_install_headers(bigtable_client
                                     include/google/cloud/bigtable)

    # Setup global variables used in the following *.in files.
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${BIGTABLE_CLIENT_VERSION_MAJOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${BIGTABLE_CLIENT_VERSION_MINOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${BIGTABLE_CLIENT_VERSION_PATCH})
    set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Bigtable C++ Client Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
        "Provides C++ APIs to access Google Cloud Bigtable.")
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-lbigtable_client -lbigtable_protos")

    # Create and install the pkg-config files.
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
                   "bigtable_client.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client.pc" DESTINATION
                  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Create and install the CMake configuration files.
    configure_file("config.cmake.in" "bigtable_client-config.cmake" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
                   "bigtable_client-config-version.cmake" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config-version.cmake"
              DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/cmake/bigtable_client")
endif ()
