# ~~~
# Copyright 2018 Google Inc.
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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud Bigtable C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Bigtable")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/examples"
                         "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")
set(DOXYGEN_EXCLUDE_SYMBOLS "benchmarks" "bigtable_admin_internal"
                            "bigtable_internal" "internal" "testing" "examples")

include(GoogleCloudCppCommon)

# Configure the location of proto files, particularly the googleapis protos.
list(APPEND PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis"
     "${PROJECT_SOURCE_DIR}")

# Enable unit tests
include(CTest)

# Export the version information for Bazel.
include(CreateBazelConfig)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_bigtable_mocks INTERFACE)
target_sources(
    google_cloud_cpp_bigtable_mocks
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/admin/mocks/mock_bigtable_instance_admin_connection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/admin/mocks/mock_bigtable_table_admin_connection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_data_connection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_row_reader.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_row_reader.h)
target_link_libraries(
    google_cloud_cpp_bigtable_mocks
    INTERFACE google-cloud-cpp::bigtable GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(google_cloud_cpp_bigtable_mocks
                      PROPERTIES EXPORT_NAME google-cloud-cpp::bigtable_mocks)
create_bazel_config(google_cloud_cpp_bigtable_mocks YEAR "2021")
target_include_directories(
    google_cloud_cpp_bigtable_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_bigtable_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(google-cloud-cpp::bigtable_mocks ALIAS
            google_cloud_cpp_bigtable_mocks)

# the client library
add_library(
    google_cloud_cpp_bigtable # cmake-format: sort
    admin/bigtable_instance_admin_client.cc
    admin/bigtable_instance_admin_client.h
    admin/bigtable_instance_admin_connection.cc
    admin/bigtable_instance_admin_connection.h
    admin/bigtable_instance_admin_connection_idempotency_policy.cc
    admin/bigtable_instance_admin_connection_idempotency_policy.h
    admin/bigtable_instance_admin_options.h
    admin/bigtable_table_admin_client.cc
    admin/bigtable_table_admin_client.h
    admin/bigtable_table_admin_connection.cc
    admin/bigtable_table_admin_connection.h
    admin/bigtable_table_admin_connection_idempotency_policy.cc
    admin/bigtable_table_admin_connection_idempotency_policy.h
    admin/bigtable_table_admin_options.h
    admin/internal/bigtable_instance_admin_auth_decorator.cc
    admin/internal/bigtable_instance_admin_auth_decorator.h
    admin/internal/bigtable_instance_admin_connection_impl.cc
    admin/internal/bigtable_instance_admin_connection_impl.h
    admin/internal/bigtable_instance_admin_logging_decorator.cc
    admin/internal/bigtable_instance_admin_logging_decorator.h
    admin/internal/bigtable_instance_admin_metadata_decorator.cc
    admin/internal/bigtable_instance_admin_metadata_decorator.h
    admin/internal/bigtable_instance_admin_option_defaults.cc
    admin/internal/bigtable_instance_admin_option_defaults.h
    admin/internal/bigtable_instance_admin_retry_traits.h
    admin/internal/bigtable_instance_admin_stub.cc
    admin/internal/bigtable_instance_admin_stub.h
    admin/internal/bigtable_instance_admin_stub_factory.cc
    admin/internal/bigtable_instance_admin_stub_factory.h
    admin/internal/bigtable_table_admin_auth_decorator.cc
    admin/internal/bigtable_table_admin_auth_decorator.h
    admin/internal/bigtable_table_admin_connection_impl.cc
    admin/internal/bigtable_table_admin_connection_impl.h
    admin/internal/bigtable_table_admin_logging_decorator.cc
    admin/internal/bigtable_table_admin_logging_decorator.h
    admin/internal/bigtable_table_admin_metadata_decorator.cc
    admin/internal/bigtable_table_admin_metadata_decorator.h
    admin/internal/bigtable_table_admin_option_defaults.cc
    admin/internal/bigtable_table_admin_option_defaults.h
    admin/internal/bigtable_table_admin_retry_traits.h
    admin/internal/bigtable_table_admin_stub.cc
    admin/internal/bigtable_table_admin_stub.h
    admin/internal/bigtable_table_admin_stub_factory.cc
    admin/internal/bigtable_table_admin_stub_factory.h
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
    internal/admin_client_params.cc
    internal/admin_client_params.h
    internal/async_bulk_apply.cc
    internal/async_bulk_apply.h
    internal/async_retry_op.h
    internal/async_row_reader.h
    internal/async_row_sampler.cc
    internal/async_row_sampler.h
    internal/bigtable_auth_decorator.cc
    internal/bigtable_auth_decorator.h
    internal/bigtable_channel_refresh.cc
    internal/bigtable_channel_refresh.h
    internal/bigtable_logging_decorator.cc
    internal/bigtable_logging_decorator.h
    internal/bigtable_metadata_decorator.cc
    internal/bigtable_metadata_decorator.h
    internal/bigtable_round_robin.cc
    internal/bigtable_round_robin.h
    internal/bigtable_stub.cc
    internal/bigtable_stub.h
    internal/bigtable_stub_factory.cc
    internal/bigtable_stub_factory.h
    internal/bulk_mutator.cc
    internal/bulk_mutator.h
    internal/client_options_defaults.h
    internal/common_client.h
    internal/connection_refresh_state.cc
    internal/connection_refresh_state.h
    internal/convert_policies.cc
    internal/convert_policies.h
    internal/data_connection.cc
    internal/data_connection.h
    internal/data_connection_impl.cc
    internal/data_connection_impl.h
    internal/defaults.cc
    internal/defaults.h
    internal/google_bytes_traits.cc
    internal/google_bytes_traits.h
    internal/legacy_row_reader.cc
    internal/legacy_row_reader.h
    internal/logging_data_client.cc
    internal/logging_data_client.h
    internal/prefix_range_end.cc
    internal/prefix_range_end.h
    internal/readrowsparser.cc
    internal/readrowsparser.h
    internal/row_reader_impl.h
    internal/rpc_policy_parameters.h
    internal/rpc_policy_parameters.inc
    internal/unary_client_utils.h
    internal/wait_for_consistency.cc
    internal/wait_for_consistency.h
    metadata_update_policy.cc
    metadata_update_policy.h
    mutation_batcher.cc
    mutation_batcher.h
    mutation_branch.h
    mutations.cc
    mutations.h
    options.h
    polling_policy.cc
    polling_policy.h
    read_modify_write_rule.h
    resource_names.cc
    resource_names.h
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
    google_cloud_cpp_bigtable
    PUBLIC absl::memory
           google-cloud-cpp::bigtable_protos
           google-cloud-cpp::common
           google-cloud-cpp::grpc_utils
           gRPC::grpc++
           gRPC::grpc
           protobuf::libprotobuf)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigtable)
target_include_directories(
    google_cloud_cpp_bigtable
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_bigtable
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_bigtable
    PROPERTIES EXPORT_NAME "google-cloud-cpp::bigtable"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::bigtable ALIAS google_cloud_cpp_bigtable)

include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_bigtable)

if (BUILD_TESTING)
    add_library(
        bigtable_client_testing # cmake-format: sort
        testing/cleanup_stale_resources.cc
        testing/cleanup_stale_resources.h
        testing/embedded_server_test_fixture.cc
        testing/embedded_server_test_fixture.h
        testing/inprocess_data_client.cc
        testing/inprocess_data_client.h
        testing/mock_async_failing_rpc_factory.h
        testing/mock_bigtable_stub.h
        testing/mock_data_client.h
        testing/mock_mutate_rows_reader.h
        testing/mock_policies.h
        testing/mock_read_rows_reader.h
        testing/mock_response_reader.h
        testing/mock_sample_row_keys_reader.h
        testing/random_names.cc
        testing/random_names.h
        testing/table_integration_test.cc
        testing/table_integration_test.h
        testing/table_test_fixture.cc
        testing/table_test_fixture.h)
    target_link_libraries(
        bigtable_client_testing
        PUBLIC absl::memory
               google-cloud-cpp::bigtable
               google-cloud-cpp::bigtable_mocks
               google-cloud-cpp::bigtable_protos
               google-cloud-cpp::common
               google-cloud-cpp::grpc_utils
               google_cloud_cpp_testing
               google_cloud_cpp_testing_grpc
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
        async_read_stream_test.cc
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
        internal/admin_client_params_test.cc
        internal/async_bulk_apply_test.cc
        internal/async_row_reader_test.cc
        internal/async_row_sampler_test.cc
        internal/bigtable_channel_refresh_test.cc
        internal/bigtable_round_robin_test.cc
        internal/bigtable_stub_factory_test.cc
        internal/bulk_mutator_test.cc
        internal/connection_refresh_state_test.cc
        internal/convert_policies_test.cc
        internal/data_connection_impl_test.cc
        internal/defaults_test.cc
        internal/google_bytes_traits_test.cc
        internal/legacy_row_reader_test.cc
        internal/logging_data_client_test.cc
        internal/prefix_range_end_test.cc
        internal/wait_for_consistency_test.cc
        metadata_update_policy_test.cc
        mocks/mock_row_reader_test.cc
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
        table_test.cc
        testing/cleanup_stale_resources_test.cc
        testing/random_names_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("bigtable_client_unit_tests.bzl"
                         "bigtable_client_unit_tests" YEAR "2018")

    # Append this unit test after exporting to Bazel because it requires special
    # treatment
    list(APPEND bigtable_client_unit_tests internal/readrowsparser_test.cc)

    foreach (fname ${bigtable_client_unit_tests})
        google_cloud_cpp_add_executable(target "bigtable" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE absl::memory
                    bigtable_client_testing
                    google-cloud-cpp::bigtable
                    google-cloud-cpp::bigtable_protos
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::common
                    google-cloud-cpp::grpc_utils
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

# Only compile integration tests and benchmarks if testing is enabled.
if (BUILD_TESTING)
    add_subdirectory(admin/integration_tests)
    add_subdirectory(benchmarks)
    add_subdirectory(tests)
endif ()

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(bigtable_quickstart "quickstart/quickstart.cc")
    target_link_libraries(bigtable_quickstart
                          PRIVATE google-cloud-cpp::bigtable)
    google_cloud_cpp_add_common_options(bigtable_quickstart)
    add_test(
        NAME bigtable_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:bigtable_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID
            GOOGLE_CLOUD_CPP_BIGTABLE_TEST_QUICKSTART_TABLE)
    set_tests_properties(bigtable_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigtable-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigtable"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_bigtable
    EXPORT google_cloud_cpp_bigtable-targets
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
    TARGETS google_cloud_cpp_bigtable
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_bigtable"
                                 "include/google/cloud/bigtable")
google_cloud_cpp_install_headers("google_cloud_cpp_bigtable_mocks"
                                 "include/google/cloud/bigtable")

google_cloud_cpp_add_pkgconfig(
    bigtable
    "The Google Cloud Bigtable C++ Client Library"
    "Provides C++ APIs to access Google Cloud Bigtable."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_bigtable_protos"
    " absl_memory")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_bigtable-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigtable-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigtable-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigtable-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigtable"
    COMPONENT google_cloud_cpp_development)
