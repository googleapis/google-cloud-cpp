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

# TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl release
set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
find_package(absl CONFIG REQUIRED)
unset(FPHSA_NAME_MISMATCHED)

set(DOXYGEN_PROJECT_NAME "Google Cloud Bigtable C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Bigtable")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/examples"
                         "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v${PROJECT_VERSION_MAJOR}"
                       "BIGTABLE_CLIENT_NS=v${PROJECT_VERSION_MAJOR}")

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
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")

include(GoogleCloudCppCommon)

# Configure the location of proto files, particulary the googleapis protos.
list(APPEND PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis"
     "${PROJECT_SOURCE_DIR}")

# Create a library with the generated files from the relevant protos.
add_library(bigtable_protos INTERFACE)
target_link_libraries(bigtable_protos INTERFACE googleapis-c++::bigtable_protos)
add_library(bigtable::protos ALIAS bigtable_protos)

find_package(gRPC)

# Enable unit tests
include(CTest)

# Generate the version information from the CMake values.
configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)

# the client library
add_library(
    bigtable_client # cmake-format: sort
    admin_client.cc
    admin_client.h
    app_profile_config.cc
    app_profile_config.h
    async_row_reader.h
    cell.h
    client_options.cc
    client_options.h
    cluster_config.cc
    cluster_config.h
    cluster_list_responses.h
    column_family.h
    completion_queue.h
    data_client.cc
    data_client.h
    expr.cc
    expr.h
    filters.h
    iam_binding.cc
    iam_binding.h
    iam_policy.cc
    iam_policy.h
    idempotent_mutation_policy.cc
    idempotent_mutation_policy.h
    instance_admin.cc
    instance_admin.h
    instance_admin_client.cc
    instance_admin_client.h
    instance_config.cc
    instance_config.h
    instance_list_responses.h
    instance_update_config.cc
    instance_update_config.h
    internal/async_bulk_apply.cc
    internal/async_bulk_apply.h
    internal/async_longrunning_op.h
    internal/async_poll_op.h
    internal/async_retry_multi_page.h
    internal/async_retry_op.h
    internal/async_retry_unary_rpc_and_poll.h
    internal/bulk_mutator.cc
    internal/bulk_mutator.h
    internal/client_options_defaults.h
    internal/common_client.cc
    internal/common_client.h
    internal/google_bytes_traits.cc
    internal/google_bytes_traits.h
    internal/logging_admin_client.cc
    internal/logging_admin_client.h
    internal/logging_data_client.cc
    internal/logging_data_client.h
    internal/logging_instance_admin_client.cc
    internal/logging_instance_admin_client.h
    internal/prefix_range_end.cc
    internal/prefix_range_end.h
    internal/readrowsparser.cc
    internal/readrowsparser.h
    internal/rowreaderiterator.cc
    internal/rowreaderiterator.h
    internal/rpc_policy_parameters.h
    internal/rpc_policy_parameters.inc
    internal/unary_client_utils.h
    metadata_update_policy.cc
    metadata_update_policy.h
    mutation_batcher.cc
    mutation_batcher.h
    mutations.cc
    mutations.h
    polling_policy.cc
    polling_policy.h
    read_modify_write_rule.h
    row.h
    row_key.h
    row_key_sample.h
    row_range.cc
    row_range.h
    row_reader.cc
    row_reader.h
    row_set.cc
    row_set.h
    rpc_backoff_policy.cc
    rpc_backoff_policy.h
    rpc_retry_policy.cc
    rpc_retry_policy.h
    table.cc
    table.h
    table_admin.cc
    table_admin.h
    table_config.cc
    table_config.h
    version.cc
    version.h
    version_info.h)
target_link_libraries(
    bigtable_client
    PUBLIC absl::memory
           bigtable_protos
           google_cloud_cpp_common
           google_cloud_cpp_grpc_utils
           gRPC::grpc++
           gRPC::grpc
           protobuf::libprotobuf)
google_cloud_cpp_add_common_options(bigtable_client)
target_include_directories(
    bigtable_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    bigtable_client PROPERTIES VERSION ${PROJECT_VERSION}
                               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(bigtable::client ALIAS bigtable_client)

include(CreateBazelConfig)
create_bazel_config(bigtable_client)

if (BUILD_TESTING)
    add_library(
        bigtable_client_testing # cmake-format: sort
        testing/embedded_server_test_fixture.cc
        testing/embedded_server_test_fixture.h
        testing/inprocess_admin_client.cc
        testing/inprocess_admin_client.h
        testing/inprocess_data_client.cc
        testing/inprocess_data_client.h
        testing/mock_admin_client.h
        testing/mock_async_failing_rpc_factory.h
        testing/mock_data_client.h
        testing/mock_instance_admin_client.h
        testing/mock_mutate_rows_reader.h
        testing/mock_read_rows_reader.h
        testing/mock_response_reader.h
        testing/mock_sample_row_keys_reader.h
        testing/table_integration_test.cc
        testing/table_integration_test.h
        testing/table_test_fixture.cc
        testing/table_test_fixture.h)
    target_link_libraries(
        bigtable_client_testing
        PUBLIC absl::memory
               bigtable_client
               bigtable_protos
               google_cloud_cpp_common
               google_cloud_cpp_grpc_utils
               GTest::gmock_main
               GTest::gmock
               GTest::gtest
               gRPC::grpc++
               gRPC::grpc
               protobuf::libprotobuf)
    google_cloud_cpp_add_common_options(bigtable_client_testing)

    create_bazel_config(bigtable_client_testing)

    # List the unit tests, then setup the targets and dependencies.
    set(bigtable_client_unit_tests
        # cmake-format: sort
        admin_client_test.cc
        app_profile_config_test.cc
        async_list_app_profiles_test.cc
        async_list_clusters_test.cc
        async_list_instances_test.cc
        async_read_stream_test.cc
        async_row_reader_test.cc
        bigtable_version_test.cc
        cell_test.cc
        client_options_test.cc
        cluster_config_test.cc
        column_family_test.cc
        data_client_test.cc
        expr_test.cc
        filters_test.cc
        force_sanitizer_failures_test.cc
        iam_binding_test.cc
        iam_policy_test.cc
        idempotent_mutation_policy_test.cc
        instance_admin_client_test.cc
        instance_admin_test.cc
        instance_config_test.cc
        instance_update_config_test.cc
        internal/async_bulk_apply_test.cc
        internal/async_longrunning_op_test.cc
        internal/async_poll_op_test.cc
        internal/async_retry_multi_page_test.cc
        internal/async_retry_unary_rpc_and_poll_test.cc
        internal/bulk_mutator_test.cc
        internal/common_client_test.cc
        internal/google_bytes_traits_test.cc
        internal/logging_admin_client_test.cc
        internal/logging_data_client_test.cc
        internal/logging_instance_admin_client_test.cc
        internal/prefix_range_end_test.cc
        metadata_update_policy_test.cc
        mutation_batcher_test.cc
        mutations_test.cc
        polling_policy_test.cc
        read_modify_write_rule_test.cc
        row_range_test.cc
        row_reader_test.cc
        row_set_test.cc
        row_test.cc
        rpc_backoff_policy_test.cc
        rpc_retry_policy_test.cc
        table_admin_test.cc
        table_apply_test.cc
        table_bulk_apply_test.cc
        table_check_and_mutate_row_test.cc
        table_config_test.cc
        table_readmodifywriterow_test.cc
        table_readrow_test.cc
        table_readrows_test.cc
        table_sample_row_keys_test.cc
        table_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("bigtable_client_unit_tests.bzl"
                         "bigtable_client_unit_tests")

    # Append this unit test after exporting to Bazel because it requires special
    # treatment
    list(APPEND bigtable_client_unit_tests internal/readrowsparser_test.cc)

    foreach (fname ${bigtable_client_unit_tests})
        google_cloud_cpp_add_executable(target "bigtable" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE absl::memory
                    bigtable_client_testing
                    bigtable_client
                    bigtable_protos
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google_cloud_cpp_common
                    google_cloud_cpp_grpc_utils
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

option(GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS
       "If set, enable tests that force errors detected by the static analyzer."
       "")
mark_as_advanced(GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS)
if (GOOGLE_CLOUD_CPP_FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(
        bigtable_client_force_sanitizer_failures_test
        PRIVATE -DBIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif ()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

add_subdirectory(benchmarks)

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    # The examples are more readable if we use exceptions for error handling. We
    # had to tradeoff readability vs. "making them compile everywhere".
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS bigtable_protos
    EXPORT bigtable-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS bigtable_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT bigtable-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bigtable_client")

install(
    TARGETS bigtable_client
    EXPORT bigtable-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS bigtable_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(bigtable_client include/google/cloud/bigtable)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Bigtable C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Bigtable.")
set(GOOGLE_CLOUD_CPP_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common googleapis_cpp_bigtable_protos"
)
google_cloud_cpp_absl_pkg_config(absl_pkg_config bigtable_client)
string(CONCAT GOOGLE_CLOUD_CPP_PC_LIBS "-lbigtable_client" ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "bigtable_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "bigtable_client-config.cmake" @ONLY)
write_basic_package_version_file(
    "bigtable_client-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bigtable_client")
