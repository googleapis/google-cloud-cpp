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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud Pub/Sub C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Pub/Sub")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/samples ${CMAKE_CURRENT_SOURCE_DIR}/quickstart,
    ${CMAKE_CURRENT_SOURCE_DIR}/admin/samples)
set(DOXYGEN_EXCLUDE_SYMBOLS
    "internal" "pubsub_admin_internal" "pubsub_internal" "pubsub_testing"
    "examples" "samples")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("pubsub" DEPENDS cloud-docs
                                 google-cloud-cpp::pubsub_protos)
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES AND TARGET pubsub-docs)
    add_dependencies(pubsub-docs pubsub_samples_protos)
endif ()

include(GoogleCloudCppCommon)

include(GoogleCloudCppLibrary)
google_cloud_cpp_add_library_protos(pubsub)

add_library(
    google_cloud_cpp_pubsub # cmake-format: sort
    ack_handler.cc
    ack_handler.h
    admin/internal/subscription_admin_auth_decorator.cc
    admin/internal/subscription_admin_auth_decorator.h
    admin/internal/subscription_admin_connection_impl.cc
    admin/internal/subscription_admin_connection_impl.h
    admin/internal/subscription_admin_logging_decorator.cc
    admin/internal/subscription_admin_logging_decorator.h
    admin/internal/subscription_admin_metadata_decorator.cc
    admin/internal/subscription_admin_metadata_decorator.h
    admin/internal/subscription_admin_option_defaults.cc
    admin/internal/subscription_admin_option_defaults.h
    admin/internal/subscription_admin_retry_traits.h
    admin/internal/subscription_admin_stub.cc
    admin/internal/subscription_admin_stub.h
    admin/internal/subscription_admin_stub_factory.cc
    admin/internal/subscription_admin_stub_factory.h
    admin/internal/subscription_admin_tracing_connection.cc
    admin/internal/subscription_admin_tracing_connection.h
    admin/internal/subscription_admin_tracing_stub.cc
    admin/internal/subscription_admin_tracing_stub.h
    admin/internal/topic_admin_auth_decorator.cc
    admin/internal/topic_admin_auth_decorator.h
    admin/internal/topic_admin_connection_impl.cc
    admin/internal/topic_admin_connection_impl.h
    admin/internal/topic_admin_logging_decorator.cc
    admin/internal/topic_admin_logging_decorator.h
    admin/internal/topic_admin_metadata_decorator.cc
    admin/internal/topic_admin_metadata_decorator.h
    admin/internal/topic_admin_option_defaults.cc
    admin/internal/topic_admin_option_defaults.h
    admin/internal/topic_admin_retry_traits.h
    admin/internal/topic_admin_stub.cc
    admin/internal/topic_admin_stub.h
    admin/internal/topic_admin_stub_factory.cc
    admin/internal/topic_admin_stub_factory.h
    admin/internal/topic_admin_tracing_connection.cc
    admin/internal/topic_admin_tracing_connection.h
    admin/internal/topic_admin_tracing_stub.cc
    admin/internal/topic_admin_tracing_stub.h
    admin/subscription_admin_client.cc
    admin/subscription_admin_client.h
    admin/subscription_admin_connection.cc
    admin/subscription_admin_connection.h
    admin/subscription_admin_connection_idempotency_policy.cc
    admin/subscription_admin_connection_idempotency_policy.h
    admin/subscription_admin_options.h
    admin/topic_admin_client.cc
    admin/topic_admin_client.h
    admin/topic_admin_connection.cc
    admin/topic_admin_connection.h
    admin/topic_admin_connection_idempotency_policy.cc
    admin/topic_admin_connection_idempotency_policy.h
    admin/topic_admin_options.h
    application_callback.h
    backoff_policy.h
    blocking_publisher.cc
    blocking_publisher.h
    blocking_publisher_connection.cc
    blocking_publisher_connection.h
    connection_options.cc
    connection_options.h
    exactly_once_ack_handler.cc
    exactly_once_ack_handler.h
    internal/ack_handler_wrapper.cc
    internal/ack_handler_wrapper.h
    internal/batch_callback.h
    internal/batch_callback_wrapper.h
    internal/batch_sink.h
    internal/batching_publisher_connection.cc
    internal/batching_publisher_connection.h
    internal/batching_publisher_tracing_connection.cc
    internal/batching_publisher_tracing_connection.h
    internal/blocking_publisher_connection_impl.cc
    internal/blocking_publisher_connection_impl.h
    internal/blocking_publisher_tracing_connection.cc
    internal/blocking_publisher_tracing_connection.h
    internal/containing_publisher_connection.h
    internal/create_channel.cc
    internal/create_channel.h
    internal/default_batch_callback.h
    internal/default_batch_sink.cc
    internal/default_batch_sink.h
    internal/default_message_callback.h
    internal/default_pull_ack_handler.cc
    internal/default_pull_ack_handler.h
    internal/default_pull_lease_manager.cc
    internal/default_pull_lease_manager.h
    internal/defaults.cc
    internal/defaults.h
    internal/exactly_once_policies.cc
    internal/exactly_once_policies.h
    internal/extend_leases_with_retry.cc
    internal/extend_leases_with_retry.h
    internal/flow_controlled_publisher_connection.cc
    internal/flow_controlled_publisher_connection.h
    internal/flow_controlled_publisher_tracing_connection.cc
    internal/flow_controlled_publisher_tracing_connection.h
    internal/message_callback.h
    internal/message_callback_wrapper.h
    internal/message_carrier.cc
    internal/message_carrier.h
    internal/message_propagator.cc
    internal/message_propagator.h
    internal/noop_message_callback.h
    internal/ordering_key_publisher_connection.cc
    internal/ordering_key_publisher_connection.h
    internal/publisher_auth_decorator.cc
    internal/publisher_auth_decorator.h
    internal/publisher_logging_decorator.cc
    internal/publisher_logging_decorator.h
    internal/publisher_metadata_decorator.cc
    internal/publisher_metadata_decorator.h
    internal/publisher_round_robin_decorator.cc
    internal/publisher_round_robin_decorator.h
    internal/publisher_stub.cc
    internal/publisher_stub.h
    internal/publisher_stub_factory.cc
    internal/publisher_stub_factory.h
    internal/publisher_tracing_connection.cc
    internal/publisher_tracing_connection.h
    internal/publisher_tracing_stub.cc
    internal/publisher_tracing_stub.h
    internal/pull_ack_handler_factory.cc
    internal/pull_ack_handler_factory.h
    internal/pull_lease_manager.h
    internal/pull_lease_manager_factory.cc
    internal/pull_lease_manager_factory.h
    internal/rejects_with_ordering_key.cc
    internal/rejects_with_ordering_key.h
    internal/schema_auth_decorator.cc
    internal/schema_auth_decorator.h
    internal/schema_connection_impl.cc
    internal/schema_connection_impl.h
    internal/schema_logging_decorator.cc
    internal/schema_logging_decorator.h
    internal/schema_metadata_decorator.cc
    internal/schema_metadata_decorator.h
    internal/schema_option_defaults.cc
    internal/schema_option_defaults.h
    internal/schema_retry_traits.h
    internal/schema_stub.cc
    internal/schema_stub.h
    internal/schema_stub_factory.cc
    internal/schema_stub_factory.h
    internal/schema_tracing_connection.cc
    internal/schema_tracing_connection.h
    internal/schema_tracing_stub.cc
    internal/schema_tracing_stub.h
    internal/sequential_batch_sink.cc
    internal/sequential_batch_sink.h
    internal/session_shutdown_manager.cc
    internal/session_shutdown_manager.h
    internal/streaming_subscription_batch_source.cc
    internal/streaming_subscription_batch_source.h
    internal/subscriber_auth_decorator.cc
    internal/subscriber_auth_decorator.h
    internal/subscriber_connection_impl.cc
    internal/subscriber_connection_impl.h
    internal/subscriber_logging_decorator.cc
    internal/subscriber_logging_decorator.h
    internal/subscriber_metadata_decorator.cc
    internal/subscriber_metadata_decorator.h
    internal/subscriber_round_robin_decorator.cc
    internal/subscriber_round_robin_decorator.h
    internal/subscriber_stub.cc
    internal/subscriber_stub.h
    internal/subscriber_stub_factory.cc
    internal/subscriber_stub_factory.h
    internal/subscriber_tracing_connection.cc
    internal/subscriber_tracing_connection.h
    internal/subscriber_tracing_stub.cc
    internal/subscriber_tracing_stub.h
    internal/subscription_batch_source.h
    internal/subscription_concurrency_control.cc
    internal/subscription_concurrency_control.h
    internal/subscription_lease_management.cc
    internal/subscription_lease_management.h
    internal/subscription_message_queue.cc
    internal/subscription_message_queue.h
    internal/subscription_message_source.h
    internal/subscription_session.cc
    internal/subscription_session.h
    internal/tracing_batch_callback.cc
    internal/tracing_batch_callback.h
    internal/tracing_batch_sink.cc
    internal/tracing_batch_sink.h
    internal/tracing_helpers.cc
    internal/tracing_helpers.h
    internal/tracing_pull_ack_handler.cc
    internal/tracing_pull_ack_handler.h
    internal/tracing_pull_lease_manager.cc
    internal/tracing_pull_lease_manager.h
    message.cc
    message.h
    options.cc
    options.h
    publisher.h
    publisher_connection.cc
    publisher_connection.h
    publisher_options.cc
    publisher_options.h
    pull_ack_handler.cc
    pull_ack_handler.h
    pull_response.h
    retry_policy.h
    schema.cc
    schema.h
    schema_client.cc
    schema_client.h
    schema_connection.cc
    schema_connection.h
    schema_connection_idempotency_policy.cc
    schema_connection_idempotency_policy.h
    schema_options.h
    snapshot.cc
    snapshot.h
    snapshot_builder.cc
    snapshot_builder.h
    subscriber.cc
    subscriber.h
    subscriber_connection.cc
    subscriber_connection.h
    subscriber_options.cc
    subscriber_options.h
    subscription.cc
    subscription.h
    subscription_admin_client.cc
    subscription_admin_client.h
    subscription_admin_connection.cc
    subscription_admin_connection.h
    subscription_builder.cc
    subscription_builder.h
    topic.cc
    topic.h
    topic_admin_client.cc
    topic_admin_client.h
    topic_admin_connection.cc
    topic_admin_connection.h
    topic_builder.cc
    topic_builder.h
    version.h
    version_info.h)
target_include_directories(
    google_cloud_cpp_pubsub
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_pubsub
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::pubsub_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_pubsub)
set_target_properties(
    google_cloud_cpp_pubsub
    PROPERTIES EXPORT_NAME "google-cloud-cpp::pubsub"
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_pubsub
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::pubsub ALIAS google_cloud_cpp_pubsub)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_pubsub YEAR "2020")

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT pubsub-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsub"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_pubsub google_cloud_cpp_pubsub_protos
    EXPORT pubsub-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_pubsub"
                                 "include/google/cloud/pubsub")

google_cloud_cpp_add_pkgconfig(
    pubsub
    "The Google Cloud Pub/Sub C++ Client Library"
    "Provides C++ APIs to access Google Cloud Pub/Sub."
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_common"
    "google_cloud_cpp_pubsub_protos"
    "absl_str_format")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
set(GOOGLE_CLOUD_CPP_CONFIG_LIBRARY "pubsub")
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.cmake.in"
               "google_cloud_cpp_pubsub-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_pubsub-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsub-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsub-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsub"
    COMPONENT google_cloud_cpp_development)

if (GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Create a header-only library for the mocks. We use a CMake `INTERFACE`
    # library for these, a regular library would not work on macOS (where the
    # library needs at least one .o file). Unfortunately INTERFACE libraries are
    # a bit weird in that they need absolute paths for their sources.
    add_library(google_cloud_cpp_pubsub_mocks INTERFACE)
    set(google_cloud_cpp_pubsub_mocks_hdrs
        # cmake-format: sort
        admin/mocks/mock_subscription_admin_connection.h
        admin/mocks/mock_topic_admin_connection.h
        mocks/mock_ack_handler.h
        mocks/mock_blocking_publisher_connection.h
        mocks/mock_exactly_once_ack_handler.h
        mocks/mock_publisher_connection.h
        mocks/mock_pull_ack_handler.h
        mocks/mock_schema_connection.h
        mocks/mock_subscriber_connection.h
        mocks/mock_subscription_admin_connection.h
        mocks/mock_topic_admin_connection.h)
    export_list_to_bazel("google_cloud_cpp_pubsub_mocks.bzl"
                         "google_cloud_cpp_pubsub_mocks_hdrs" YEAR "2020")
    # Add the prefix to each file name for cmake
    set(cmake_google_cloud_cpp_pubsub_mocks_hdrs)
    foreach (fname ${google_cloud_cpp_pubsub_mocks_hdrs})
        list(APPEND cmake_google_cloud_cpp_pubsub_mocks_hdrs
             "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${fname}>")
    endforeach ()
    target_sources(google_cloud_cpp_pubsub_mocks
                   INTERFACE ${cmake_google_cloud_cpp_pubsub_mocks_hdrs})
    target_link_libraries(
        google_cloud_cpp_pubsub_mocks
        INTERFACE google-cloud-cpp::pubsub GTest::gmock_main GTest::gmock
                  GTest::gtest)
    set_target_properties(google_cloud_cpp_pubsub_mocks
                          PROPERTIES EXPORT_NAME google-cloud-cpp::pubsub_mocks)
    target_include_directories(
        google_cloud_cpp_pubsub_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    set_target_properties(
        google_cloud_cpp_pubsub_mocks
        PROPERTIES EXPORT_NAME "google-cloud-cpp::pubsub_mocks")
    target_compile_options(google_cloud_cpp_pubsub_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    add_library(google-cloud-cpp::pubsub_mocks ALIAS
                google_cloud_cpp_pubsub_mocks)

    google_cloud_cpp_install_mocks(pubsub "Google Cloud Pub/Sub")
endif ()

# Define the tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_pubsub_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    add_library(
        pubsub_client_testing # cmake-format: sort
        testing/fake_streaming_pull.cc
        testing/fake_streaming_pull.h
        testing/mock_batch_callback.h
        testing/mock_batch_sink.h
        testing/mock_message_callback.h
        testing/mock_publisher_stub.h
        testing/mock_pull_lease_manager.h
        testing/mock_subscriber_stub.h
        testing/mock_subscription_batch_source.h
        testing/mock_subscription_message_source.h
        testing/random_names.cc
        testing/random_names.h
        testing/test_retry_policies.cc
        testing/test_retry_policies.h)
    target_link_libraries(
        pubsub_client_testing
        PUBLIC google-cloud-cpp::pubsub google-cloud-cpp::grpc_utils
               google-cloud-cpp::common google-cloud-cpp::pubsub_protos
               GTest::gmock GTest::gtest)
    google_cloud_cpp_add_common_options(pubsub_client_testing)
    target_compile_options(pubsub_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    create_bazel_config(pubsub_client_testing YEAR "2020")

    set(pubsub_client_unit_tests
        # cmake-format: sort
        ack_handler_test.cc
        blocking_publisher_connection_test.cc
        blocking_publisher_test.cc
        exactly_once_ack_handler_test.cc
        internal/ack_handler_wrapper_test.cc
        internal/batching_publisher_connection_test.cc
        internal/batching_publisher_tracing_connection_test.cc
        internal/blocking_publisher_tracing_connection_test.cc
        internal/default_batch_sink_test.cc
        internal/default_pull_ack_handler_test.cc
        internal/default_pull_lease_manager_test.cc
        internal/defaults_test.cc
        internal/exactly_once_policies_test.cc
        internal/extend_leases_with_retry_test.cc
        internal/flow_controlled_publisher_connection_test.cc
        internal/flow_controlled_publisher_tracing_connection_test.cc
        internal/message_carrier_test.cc
        internal/message_propagator_test.cc
        internal/ordering_key_publisher_connection_test.cc
        internal/publisher_stub_factory_test.cc
        internal/publisher_tracing_connection_test.cc
        internal/pull_ack_handler_factory_test.cc
        internal/pull_lease_manager_factory_test.cc
        internal/rejects_with_ordering_key_test.cc
        internal/sequential_batch_sink_test.cc
        internal/session_shutdown_manager_test.cc
        internal/streaming_subscription_batch_source_test.cc
        internal/subscriber_connection_impl_test.cc
        internal/subscriber_stub_factory_test.cc
        internal/subscriber_tracing_connection_test.cc
        internal/subscription_concurrency_control_test.cc
        internal/subscription_lease_management_test.cc
        internal/subscription_message_queue_test.cc
        internal/subscription_session_test.cc
        internal/tracing_batch_callback_test.cc
        internal/tracing_batch_sink_test.cc
        internal/tracing_helpers_test.cc
        internal/tracing_pull_ack_handler_test.cc
        internal/tracing_pull_lease_manager_test.cc
        message_test.cc
        options_test.cc
        publisher_connection_test.cc
        publisher_options_test.cc
        publisher_test.cc
        pull_ack_handler_test.cc
        schema_test.cc
        snapshot_builder_test.cc
        snapshot_test.cc
        subscriber_connection_test.cc
        subscriber_options_test.cc
        subscriber_test.cc
        subscription_admin_client_test.cc
        subscription_admin_connection_test.cc
        subscription_builder_test.cc
        subscription_test.cc
        topic_admin_client_test.cc
        topic_admin_connection_test.cc
        topic_builder_test.cc
        topic_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("pubsub_client_unit_tests.bzl"
                         "pubsub_client_unit_tests" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${pubsub_client_unit_tests})
        google_cloud_cpp_add_executable(target "pubsub" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE pubsub_client_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::pubsub_mocks
                    google-cloud-cpp::pubsub
                    absl::str_format
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_pubsub_client_define_tests()
    add_subdirectory(benchmarks)
    add_subdirectory(admin/samples)
    add_subdirectory(integration_tests)
endif (BUILD_TESTING)

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_executable(pubsub_quickstart "quickstart/quickstart.cc")
    target_link_libraries(pubsub_quickstart PRIVATE google-cloud-cpp::pubsub)
    google_cloud_cpp_add_common_options(pubsub_quickstart)
    add_test(
        NAME pubsub_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:pubsub_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC)
    set_tests_properties(pubsub_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
