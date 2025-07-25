# ~~~
# Copyright 2019 Google LLC
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
set(DOXYGEN_PROJECT_NAME "Google Cloud Spanner C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Spanner")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    ${CMAKE_CURRENT_SOURCE_DIR}/admin/samples
    ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "spanner_admin_internal"
                            "spanner_internal" "spanner_testing" "examples")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("spanner" DEPENDS cloud-docs
                                 google-cloud-cpp::spanner_protos)

include(GoogleCloudCppCommon)

include(GoogleCloudCppLibrary)
google_cloud_cpp_add_library_protos(spanner)

add_library(
    google_cloud_cpp_spanner # cmake-format: sort
    admin/database_admin_client.cc
    admin/database_admin_client.h
    admin/database_admin_connection.cc
    admin/database_admin_connection.h
    admin/database_admin_connection_idempotency_policy.cc
    admin/database_admin_connection_idempotency_policy.h
    admin/database_admin_options.h
    admin/instance_admin_client.cc
    admin/instance_admin_client.h
    admin/instance_admin_connection.cc
    admin/instance_admin_connection.h
    admin/instance_admin_connection_idempotency_policy.cc
    admin/instance_admin_connection_idempotency_policy.h
    admin/instance_admin_options.h
    admin/internal/database_admin_auth_decorator.cc
    admin/internal/database_admin_auth_decorator.h
    admin/internal/database_admin_connection_impl.cc
    admin/internal/database_admin_connection_impl.h
    admin/internal/database_admin_logging_decorator.cc
    admin/internal/database_admin_logging_decorator.h
    admin/internal/database_admin_metadata_decorator.cc
    admin/internal/database_admin_metadata_decorator.h
    admin/internal/database_admin_option_defaults.cc
    admin/internal/database_admin_option_defaults.h
    admin/internal/database_admin_retry_traits.h
    admin/internal/database_admin_stub.cc
    admin/internal/database_admin_stub.h
    admin/internal/database_admin_stub_factory.cc
    admin/internal/database_admin_stub_factory.h
    admin/internal/database_admin_tracing_connection.cc
    admin/internal/database_admin_tracing_connection.h
    admin/internal/database_admin_tracing_stub.cc
    admin/internal/database_admin_tracing_stub.h
    admin/internal/instance_admin_auth_decorator.cc
    admin/internal/instance_admin_auth_decorator.h
    admin/internal/instance_admin_connection_impl.cc
    admin/internal/instance_admin_connection_impl.h
    admin/internal/instance_admin_logging_decorator.cc
    admin/internal/instance_admin_logging_decorator.h
    admin/internal/instance_admin_metadata_decorator.cc
    admin/internal/instance_admin_metadata_decorator.h
    admin/internal/instance_admin_option_defaults.cc
    admin/internal/instance_admin_option_defaults.h
    admin/internal/instance_admin_retry_traits.h
    admin/internal/instance_admin_stub.cc
    admin/internal/instance_admin_stub.h
    admin/internal/instance_admin_stub_factory.cc
    admin/internal/instance_admin_stub_factory.h
    admin/internal/instance_admin_tracing_connection.cc
    admin/internal/instance_admin_tracing_connection.h
    admin/internal/instance_admin_tracing_stub.cc
    admin/internal/instance_admin_tracing_stub.h
    admin/retry_traits.h
    backoff_policy.h
    backup.cc
    backup.h
    batch_dml_result.h
    bytes.cc
    bytes.h
    client.cc
    client.h
    client_options.h
    commit_options.cc
    commit_options.h
    commit_result.h
    connection.cc
    connection.h
    connection_options.cc
    connection_options.h
    create_instance_request_builder.h
    database.cc
    database.h
    database_admin_client.cc
    database_admin_client.h
    database_admin_connection.cc
    database_admin_connection.h
    date.h
    directed_read_replicas.h
    encryption_config.h
    iam_updater.h
    instance.cc
    instance.h
    instance_admin_client.cc
    instance_admin_client.h
    instance_admin_connection.cc
    instance_admin_connection.h
    internal/channel.h
    internal/connection_impl.cc
    internal/connection_impl.h
    internal/database_admin_logging.cc
    internal/database_admin_logging.h
    internal/database_admin_metadata.cc
    internal/database_admin_metadata.h
    internal/database_admin_stub.cc
    internal/database_admin_stub.h
    internal/defaults.cc
    internal/defaults.h
    internal/instance_admin_logging.cc
    internal/instance_admin_logging.h
    internal/instance_admin_metadata.cc
    internal/instance_admin_metadata.h
    internal/instance_admin_stub.cc
    internal/instance_admin_stub.h
    internal/logging_result_set_reader.cc
    internal/logging_result_set_reader.h
    internal/merge_chunk.cc
    internal/merge_chunk.h
    internal/partial_result_set_reader.h
    internal/partial_result_set_resume.cc
    internal/partial_result_set_resume.h
    internal/partial_result_set_source.cc
    internal/partial_result_set_source.h
    internal/route_to_leader.cc
    internal/route_to_leader.h
    internal/session.cc
    internal/session.h
    internal/session_pool.cc
    internal/session_pool.h
    internal/spanner_auth_decorator.cc
    internal/spanner_auth_decorator.h
    internal/spanner_logging_decorator.cc
    internal/spanner_logging_decorator.h
    internal/spanner_metadata_decorator.cc
    internal/spanner_metadata_decorator.h
    internal/spanner_stub.cc
    internal/spanner_stub.h
    internal/spanner_stub_factory.cc
    internal/spanner_stub_factory.h
    internal/spanner_tracing_stub.cc
    internal/spanner_tracing_stub.h
    internal/status_utils.cc
    internal/status_utils.h
    internal/transaction_impl.cc
    internal/transaction_impl.h
    internal/tuple_utils.h
    interval.cc
    interval.h
    json.h
    keys.cc
    keys.h
    lock_hint.h
    mutations.cc
    mutations.h
    numeric.cc
    numeric.h
    oid.h
    options.h
    order_by.h
    partition_options.cc
    partition_options.h
    partitioned_dml_result.h
    polling_policy.h
    proto_enum.h
    proto_message.h
    query_options.cc
    query_options.h
    query_partition.cc
    query_partition.h
    read_options.cc
    read_options.h
    read_partition.cc
    read_partition.h
    request_priority.h
    results.cc
    results.h
    retry_policy.h
    row.cc
    row.h
    session_pool_options.h
    sql_statement.cc
    sql_statement.h
    timestamp.cc
    timestamp.h
    tracing_options.h
    transaction.cc
    transaction.h
    update_instance_request_builder.h
    uuid.cc
    uuid.h
    value.cc
    value.h
    version.cc
    version.h
    version_info.h)
target_include_directories(
    google_cloud_cpp_spanner
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_spanner
    PUBLIC absl::fixed_array
           absl::memory
           absl::numeric
           absl::strings
           absl::time
           google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           google-cloud-cpp::spanner_protos)
set_target_properties(
    google_cloud_cpp_spanner
    PROPERTIES EXPORT_NAME "google-cloud-cpp::spanner"
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_spanner
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

google_cloud_cpp_add_common_options(google_cloud_cpp_spanner)

add_library(google-cloud-cpp::spanner ALIAS google_cloud_cpp_spanner)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_spanner YEAR "2019")

if ("experimental-http-transcoding" IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    add_library(
        google_cloud_cpp_spanner_rest # cmake-format: sort
        admin/database_admin_rest_connection.cc
        admin/database_admin_rest_connection.h
        admin/instance_admin_rest_connection.cc
        admin/instance_admin_rest_connection.h
        admin/internal/database_admin_rest_connection_impl.cc
        admin/internal/database_admin_rest_connection_impl.h
        admin/internal/database_admin_rest_logging_decorator.cc
        admin/internal/database_admin_rest_logging_decorator.h
        admin/internal/database_admin_rest_metadata_decorator.cc
        admin/internal/database_admin_rest_metadata_decorator.h
        admin/internal/database_admin_rest_stub.cc
        admin/internal/database_admin_rest_stub.h
        admin/internal/database_admin_rest_stub_factory.cc
        admin/internal/database_admin_rest_stub_factory.h
        admin/internal/instance_admin_rest_connection_impl.cc
        admin/internal/instance_admin_rest_connection_impl.h
        admin/internal/instance_admin_rest_logging_decorator.cc
        admin/internal/instance_admin_rest_logging_decorator.h
        admin/internal/instance_admin_rest_metadata_decorator.cc
        admin/internal/instance_admin_rest_metadata_decorator.h
        admin/internal/instance_admin_rest_stub.cc
        admin/internal/instance_admin_rest_stub.h
        admin/internal/instance_admin_rest_stub_factory.cc
        admin/internal/instance_admin_rest_stub_factory.h)
    target_include_directories(
        google_cloud_cpp_spanner_rest
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_link_libraries(
        google_cloud_cpp_spanner_rest
        PUBLIC google-cloud-cpp::rest_internal
               google-cloud-cpp::rest_protobuf_internal
               google-cloud-cpp::spanner)
    set_target_properties(
        google_cloud_cpp_spanner_rest
        PROPERTIES EXPORT_NAME "google-cloud-cpp::spanner_rest"
                   VERSION "${PROJECT_VERSION}"
                   SOVERSION "${PROJECT_VERSION_MAJOR}")
    target_compile_options(google_cloud_cpp_spanner_rest
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    google_cloud_cpp_add_common_options(google_cloud_cpp_spanner_rest)
    add_library(google-cloud-cpp::spanner_rest ALIAS
                google_cloud_cpp_spanner_rest)
    create_bazel_config(google_cloud_cpp_spanner_rest YEAR "2023")
endif ()

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT spanner-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_spanner"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_spanner google_cloud_cpp_spanner_protos
    EXPORT spanner-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_spanner"
                                 "include/google/cloud/spanner")

google_cloud_cpp_add_pkgconfig(
    spanner
    "The Google Cloud Spanner C++ Client Library"
    "Provides C++ APIs to access Google Cloud Spanner."
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_common"
    "google_cloud_cpp_spanner_protos"
    "absl_numeric"
    "absl_strings"
    "absl_time")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
set(GOOGLE_CLOUD_CPP_CONFIG_LIBRARY "spanner")
set(GOOGLE_CLOUD_CPP_TRANSPORT "grpc_utils")
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.cmake.in"
               "google_cloud_cpp_spanner-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_spanner-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_spanner-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_spanner-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_spanner"
    COMPONENT google_cloud_cpp_development)

if (GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Create a header-only library for the mocks. We use a CMake `INTERFACE`
    # library for these, a regular library would not work on macOS (where the
    # library needs at least one .o file). Unfortunately INTERFACE libraries are
    # a bit weird in that they need absolute paths for their sources.
    add_library(google_cloud_cpp_spanner_mocks INTERFACE)
    set(google_cloud_cpp_spanner_mocks_hdrs
        # cmake-format: sort
        admin/mocks/mock_database_admin_connection.h
        admin/mocks/mock_instance_admin_connection.h
        mocks/mock_database_admin_connection.h
        mocks/mock_instance_admin_connection.h
        mocks/mock_spanner_connection.h
        mocks/row.h)
    export_list_to_bazel("google_cloud_cpp_spanner_mocks.bzl"
                         google_cloud_cpp_spanner_mocks_hdrs YEAR "2019")
    foreach (file IN LISTS google_cloud_cpp_spanner_mocks_hdrs)
        # We use a generator expression per the recommendation in:
        # https://stackoverflow.com/a/62465051
        list(APPEND mock_files
             "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${file}>")
    endforeach ()
    target_sources(google_cloud_cpp_spanner_mocks INTERFACE ${mock_files})
    target_link_libraries(
        google_cloud_cpp_spanner_mocks INTERFACE google-cloud-cpp::spanner
                                                 GTest::gmock GTest::gtest)
    set_target_properties(
        google_cloud_cpp_spanner_mocks
        PROPERTIES EXPORT_NAME google-cloud-cpp::spanner_mocks)
    target_include_directories(
        google_cloud_cpp_spanner_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_spanner_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    add_library(google-cloud-cpp::spanner_mocks ALIAS
                google_cloud_cpp_spanner_mocks)

    google_cloud_cpp_install_mocks(spanner "Google Cloud Spanner")
endif ()

# Define the tests in a function so we have a new scope for variable names.
function (spanner_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    add_library(
        spanner_client_testing # cmake-format: sort
        testing/cleanup_stale_databases.cc
        testing/cleanup_stale_databases.h
        testing/cleanup_stale_instances.cc
        testing/cleanup_stale_instances.h
        testing/database_integration_test.cc
        testing/database_integration_test.h
        testing/debug_log.cc
        testing/debug_log.h
        testing/instance_location.cc
        testing/instance_location.h
        testing/matchers.h
        testing/mock_database_admin_stub.h
        testing/mock_instance_admin_stub.h
        testing/mock_partial_result_set_reader.h
        testing/mock_spanner_stub.h
        testing/pick_instance_config.cc
        testing/pick_instance_config.h
        testing/pick_random_instance.cc
        testing/pick_random_instance.h
        testing/random_backup_name.cc
        testing/random_backup_name.h
        testing/random_database_name.cc
        testing/random_database_name.h
        testing/random_instance_name.cc
        testing/random_instance_name.h
        testing/status_utils.cc
        testing/status_utils.h)
    target_link_libraries(
        spanner_client_testing
        PUBLIC google_cloud_cpp_testing
               google_cloud_cpp_spanner_client_testing_protos
               google-cloud-cpp::spanner_mocks
               google-cloud-cpp::spanner
               GTest::gmock_main
               GTest::gmock
               GTest::gtest)
    create_bazel_config(spanner_client_testing YEAR "2019")
    google_cloud_cpp_add_common_options(spanner_client_testing)

    target_include_directories(
        spanner_client_testing
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_compile_options(spanner_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    set(spanner_client_unit_tests
        # cmake-format: sort
        backup_test.cc
        bytes_test.cc
        client_options_test.cc
        client_test.cc
        commit_options_test.cc
        connection_options_test.cc
        create_instance_request_builder_test.cc
        database_admin_client_test.cc
        database_admin_connection_test.cc
        database_test.cc
        instance_admin_client_test.cc
        instance_admin_connection_test.cc
        instance_test.cc
        internal/connection_impl_test.cc
        internal/database_admin_logging_test.cc
        internal/database_admin_metadata_test.cc
        internal/defaults_test.cc
        internal/instance_admin_logging_test.cc
        internal/instance_admin_metadata_test.cc
        internal/logging_result_set_reader_test.cc
        internal/merge_chunk_test.cc
        internal/partial_result_set_resume_test.cc
        internal/partial_result_set_source_test.cc
        internal/route_to_leader_test.cc
        internal/session_pool_test.cc
        internal/spanner_stub_factory_test.cc
        internal/status_utils_test.cc
        internal/transaction_impl_test.cc
        internal/tuple_utils_test.cc
        interval_test.cc
        json_test.cc
        keys_test.cc
        mutations_test.cc
        numeric_test.cc
        partition_options_test.cc
        proto_enum_test.cc
        proto_message_test.cc
        query_options_test.cc
        query_partition_test.cc
        read_options_test.cc
        read_partition_test.cc
        results_test.cc
        retry_policy_test.cc
        row_test.cc
        session_pool_options_test.cc
        spanner_version_test.cc
        sql_statement_test.cc
        testing/cleanup_stale_databases_test.cc
        testing/random_database_name_test.cc
        timestamp_test.cc
        transaction_test.cc
        update_instance_request_builder_test.cc
        uuid_test.cc
        value_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_client_unit_tests.bzl"
                         "spanner_client_unit_tests" YEAR "2019")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(spanner-client-tests)

    # Generate a target for each unit test.
    foreach (fname ${spanner_client_unit_tests})
        google_cloud_cpp_add_executable(target "spanner" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE spanner_client_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::spanner
                    absl::memory
                    absl::numeric
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(spanner-client-tests ${target})
    endforeach ()
endfunction ()

# Define the benchmarks in a function so we have a new scope for variable names.
function (spanner_client_define_benchmarks)
    include(FindBenchmarkWithWorkarounds)

    set(spanner_client_benchmarks
        # cmake-format: sort
        bytes_benchmark.cc internal/merge_chunk_benchmark.cc
        numeric_benchmark.cc row_benchmark.cc)

    # Export the list of benchmarks to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_client_benchmarks.bzl"
                         "spanner_client_benchmarks" YEAR "2019")

    # Create a custom target so we can say "build all the benchmarks"
    add_custom_target(spanner-client-benchmarks)

    # Generate a target for each benchmark.
    foreach (fname ${spanner_client_benchmarks})
        google_cloud_cpp_add_executable(target "spanner" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::spanner google-cloud-cpp::spanner_mocks
                    benchmark::benchmark_main)
        google_cloud_cpp_add_common_options(${target})

        add_dependencies(spanner-client-benchmarks ${target})
    endforeach ()
endfunction ()

# Only define the tests/benchmarks if testing is enabled. Package maintainers
# may not want to build all the tests everytime they create a new package or
# when the package is installed from source.
if (BUILD_TESTING)
    spanner_client_define_tests()
    spanner_client_define_benchmarks()
    add_subdirectory(integration_tests)
    add_subdirectory(admin/integration_tests)
    add_subdirectory(admin/samples)
    add_subdirectory(benchmarks) # macro benchmarks
endif (BUILD_TESTING)

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(spanner_quickstart "quickstart/quickstart.cc")
    target_link_libraries(spanner_quickstart PRIVATE google-cloud-cpp::spanner)
    google_cloud_cpp_add_common_options(spanner_quickstart)
    add_test(
        NAME spanner_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:spanner_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID
            GOOGLE_CLOUD_CPP_SPANNER_TEST_QUICKSTART_DATABASE)
    set_tests_properties(spanner_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
