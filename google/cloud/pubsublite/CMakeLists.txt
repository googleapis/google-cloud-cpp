# ~~~
# Copyright 2021 Google LLC
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
set(DOXYGEN_PROJECT_NAME "Pub/Sub Lite API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the Pub/Sub Lite API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Experimental)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "pubsublite_internal"
                            "pubsublite_testing" "examples")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::pubsublite_protos)

include(GoogleCloudCppCommon)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/pubsublite.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/pubsublite.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_pubsublite_protos # cmake-format: sort
    ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(pubsublite_protos)
target_link_libraries(google_cloud_cpp_pubsublite_protos PUBLIC ${proto_deps})

add_library(
    google_cloud_cpp_pubsublite # cmake-format: sort
    admin_client.cc
    admin_client.h
    admin_connection.cc
    admin_connection.h
    admin_connection_idempotency_policy.cc
    admin_connection_idempotency_policy.h
    admin_options.h
    endpoint.cc
    endpoint.h
    internal/admin_auth_decorator.cc
    internal/admin_auth_decorator.h
    internal/admin_connection_impl.cc
    internal/admin_connection_impl.h
    internal/admin_logging_decorator.cc
    internal/admin_logging_decorator.h
    internal/admin_metadata_decorator.cc
    internal/admin_metadata_decorator.h
    internal/admin_option_defaults.cc
    internal/admin_option_defaults.h
    internal/admin_retry_traits.h
    internal/admin_stub.cc
    internal/admin_stub.h
    internal/admin_stub_factory.cc
    internal/admin_stub_factory.h
    internal/alarm_registry.h
    internal/alarm_registry_impl.cc
    internal/alarm_registry_impl.h
    internal/batching_options.h
    internal/cloud_region.cc
    internal/cloud_region.h
    internal/cloud_zone.cc
    internal/cloud_zone.h
    internal/cursor_auth_decorator.cc
    internal/cursor_auth_decorator.h
    internal/cursor_logging_decorator.cc
    internal/cursor_logging_decorator.h
    internal/cursor_metadata_decorator.cc
    internal/cursor_metadata_decorator.h
    internal/cursor_stub.cc
    internal/cursor_stub.h
    internal/cursor_stub_factory.cc
    internal/cursor_stub_factory.h
    internal/default_publish_message_transformer.cc
    internal/default_publish_message_transformer.h
    internal/default_routing_policy.cc
    internal/default_routing_policy.h
    internal/futures.h
    internal/location.cc
    internal/location.h
    internal/multipartition_publisher.cc
    internal/multipartition_publisher.h
    internal/partition_assignment_auth_decorator.cc
    internal/partition_assignment_auth_decorator.h
    internal/partition_assignment_logging_decorator.cc
    internal/partition_assignment_logging_decorator.h
    internal/partition_assignment_metadata_decorator.cc
    internal/partition_assignment_metadata_decorator.h
    internal/partition_assignment_stub.cc
    internal/partition_assignment_stub.h
    internal/partition_assignment_stub_factory.cc
    internal/partition_assignment_stub_factory.h
    internal/partition_publisher.cc
    internal/partition_publisher.h
    internal/publisher.h
    internal/publisher_auth_decorator.cc
    internal/publisher_auth_decorator.h
    internal/publisher_connection_impl.cc
    internal/publisher_connection_impl.h
    internal/publisher_logging_decorator.cc
    internal/publisher_logging_decorator.h
    internal/publisher_metadata_decorator.cc
    internal/publisher_metadata_decorator.h
    internal/publisher_stub.cc
    internal/publisher_stub.h
    internal/publisher_stub_factory.cc
    internal/publisher_stub_factory.h
    internal/resumable_async_streaming_read_write_rpc.h
    internal/routing_policy.h
    internal/service.h
    internal/service_composite.h
    internal/stream_factory.h
    internal/stream_retry_policy.h
    internal/subscriber_auth_decorator.cc
    internal/subscriber_auth_decorator.h
    internal/subscriber_logging_decorator.cc
    internal/subscriber_logging_decorator.h
    internal/subscriber_metadata_decorator.cc
    internal/subscriber_metadata_decorator.h
    internal/subscriber_stub.cc
    internal/subscriber_stub.h
    internal/subscriber_stub_factory.cc
    internal/subscriber_stub_factory.h
    internal/topic_stats_auth_decorator.cc
    internal/topic_stats_auth_decorator.h
    internal/topic_stats_connection_impl.cc
    internal/topic_stats_connection_impl.h
    internal/topic_stats_logging_decorator.cc
    internal/topic_stats_logging_decorator.h
    internal/topic_stats_metadata_decorator.cc
    internal/topic_stats_metadata_decorator.h
    internal/topic_stats_option_defaults.cc
    internal/topic_stats_option_defaults.h
    internal/topic_stats_retry_traits.h
    internal/topic_stats_stub.cc
    internal/topic_stats_stub.h
    internal/topic_stats_stub_factory.cc
    internal/topic_stats_stub_factory.h
    message_metadata.cc
    message_metadata.h
    options.h
    publisher_connection.cc
    publisher_connection.h
    topic.h
    topic_stats_client.cc
    topic_stats_client.h
    topic_stats_connection.cc
    topic_stats_connection.h
    topic_stats_connection_idempotency_policy.cc
    topic_stats_connection_idempotency_policy.h
    topic_stats_options.h)
target_include_directories(
    google_cloud_cpp_pubsublite
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_pubsublite
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::pubsublite_protos google-cloud-cpp::pubsub)
google_cloud_cpp_add_common_options(google_cloud_cpp_pubsublite)
set_target_properties(
    google_cloud_cpp_pubsublite
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-pubsublite
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_pubsublite
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-pubsublite ALIAS
            google_cloud_cpp_pubsublite)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_pubsublite YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_pubsublite_mocks INTERFACE)
target_sources(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_admin_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_topic_stats_connection.h)
target_link_libraries(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE google-cloud-cpp::experimental-pubsublite GTest::gmock_main
              GTest::gmock GTest::gtest)
set_target_properties(
    google_cloud_cpp_pubsublite_mocks
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-pubsublite_mocks)
create_bazel_config(google_cloud_cpp_pubsublite_mocks YEAR "2021")
target_include_directories(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_pubsublite_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# Define the tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_pubsublite_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)
    add_library(pubsublite_testing INTERFACE)
    target_sources(
        pubsublite_testing
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_alarm_registry.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_async_reader_writer.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_backoff_policy.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_publisher.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_resumable_async_reader_writer_stream.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_retry_policy.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_routing_policy.h
            ${CMAKE_CURRENT_SOURCE_DIR}/testing/mock_service.h)

    target_link_libraries(
        pubsublite_testing
        INTERFACE google-cloud-cpp::experimental-pubsublite GTest::gmock_main
                  GTest::gmock GTest::gtest)
    target_include_directories(
        pubsublite_testing
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(pubsublite_testing
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    create_bazel_config(pubsublite_testing YEAR "2022")

    set(pubsublite_unit_tests
        # cmake-format: sort
        endpoint_test.cc
        internal/alarm_registry_impl_test.cc
        internal/default_publish_message_transformer_test.cc
        internal/default_routing_policy_test.cc
        internal/location_test.cc
        internal/multipartition_publisher_test.cc
        internal/partition_publisher_test.cc
        internal/publisher_connection_impl_test.cc
        internal/resumable_async_streaming_read_write_rpc_test.cc
        internal/service_composite_test.cc
        internal/stream_factory_test.cc
        internal/stream_retry_policy_test.cc
        message_metadata_test.cc
        topic_test.cc)

    export_list_to_bazel("pubsublite_unit_tests.bzl" "pubsublite_unit_tests"
                         YEAR "2021")

    # Generate a target for each unit test.
    foreach (fname ${pubsublite_unit_tests})
        google_cloud_cpp_add_executable(target "pubsublite" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE pubsublite_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google_cloud_cpp_pubsublite
                    google_cloud_cpp_pubsublite_mocks
                    absl::str_format
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
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

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_pubsublite_client_define_tests()
endif (BUILD_TESTING)

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_executable(pubsublite_quickstart "quickstart/quickstart.cc")
    target_link_libraries(pubsublite_quickstart
                          PRIVATE google-cloud-cpp::experimental-pubsublite)
    google_cloud_cpp_add_common_options(pubsublite_quickstart)
    add_test(
        NAME pubsublite_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:pubsublite_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_TEST_ZONE)
    set_tests_properties(pubsublite_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_pubsublite-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsublite"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_pubsublite google_cloud_cpp_pubsublite_protos
    EXPORT google_cloud_cpp_pubsublite-targets
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
    TARGETS google_cloud_cpp_pubsublite google_cloud_cpp_pubsublite_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_pubsublite_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_pubsublite_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_pubsublite"
                                 "include/google/cloud/pubsublite")
google_cloud_cpp_install_headers("google_cloud_cpp_pubsublite_mocks"
                                 "include/google/cloud/pubsublite")

google_cloud_cpp_add_pkgconfig(
    pubsublite
    "The Pub/Sub Lite API C++ Client Library"
    "Provides C++ APIs to access Pub/Sub Lite API."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_pubsublite_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_pubsublite-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_pubsublite-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsublite-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsublite-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsublite"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_pubsublite_protos")
