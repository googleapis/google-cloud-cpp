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

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# the client library
add_library(bigtable_client
        admin_client.h
        admin_client.cc
        ${CMAKE_CURRENT_BINARY_DIR}/build_info.cc
        ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
        build_info.h
        cell.h
        client_options.h
        client_options.cc
        column_family.h
        data_client.h
        data_client.cc
        filters.h
        filters.cc
        grpc_error.h
        grpc_error.cc
        instance_admin_client.h
        instance_admin_client.cc
        instance_admin.h
        instance_admin.cc
        internal/bulk_mutator.h
        internal/bulk_mutator.cc
        internal/common_client.h
        internal/common_client.cc
        internal/conjunction.h
        internal/instance_admin.h
        internal/instance_admin.cc
        internal/make_unique.h
        internal/port_platform.h
        internal/prefix_range_end.h
        internal/prefix_range_end.cc
        internal/readrowsparser.h
        internal/readrowsparser.cc
        internal/rowreaderiterator.h
        internal/rowreaderiterator.cc
        internal/table.h
        internal/table.cc
        internal/table_admin.h
        internal/table_admin.cc
        internal/throw_delegate.h
        internal/throw_delegate.cc
        internal/unary_rpc_utils.h
        idempotent_mutation_policy.h
        idempotent_mutation_policy.cc
        mutations.h
        mutations.cc
        read_modify_write_rule.h
        read_modify_write_rule.cc
        row.h
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
        version.h
        version.cc)
target_link_libraries(bigtable_client PUBLIC
        bigtable_protos
        gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
        PRIVATE bigtable_common_options)
target_include_directories(bigtable_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_client PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(bigtable::client ALIAS bigtable_client)

add_library(bigtable_client_testing
        testing/chrono_literals.h
        testing/embedded_server_test_fixture.h
        testing/embedded_server_test_fixture.cc
        testing/internal_table_test_fixture.h
        testing/internal_table_test_fixture.cc
        testing/mock_data_client.h
        testing/inprocess_data_client.h
        testing/inprocess_admin_client.h
        testing/mock_response_stream.h
        testing/table_integration_test.h
        testing/table_integration_test.cc
        testing/table_test_fixture.h
        testing/table_test_fixture.cc)
target_link_libraries(bigtable_client_testing PUBLIC
        bigtable_client bigtable_protos
        gmock gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
    PRIVATE bigtable_common_options)

# List the unit tests, then setup the targets and dependencies.
set(bigtable_client_unit_tests
        admin_client_test.cc
        cell_test.cc
        client_options_test.cc
        column_family_test.cc
        data_client_test.cc
        filters_test.cc
        force_sanitizer_failures_test.cc
        grpc_error_test.cc
        idempotent_mutation_policy_test.cc
        instance_admin_client_test.cc
        instance_admin_test.cc
        internal/bulk_mutator_test.cc
        internal/instance_admin_test.cc
        internal/prefix_range_end_test.cc
        internal/readrowsparser_test.cc
        internal/table_admin_test.cc
        internal/table_test.cc
        internal/throw_delegate_test.cc
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
        rpc_retry_policy_test.cc)

foreach (fname ${bigtable_client_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "bigtable_client_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
            bigtable_client_testing bigtable_client bigtable_protos
            gmock gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
            bigtable_common_options)
    add_test(NAME ${target} COMMAND ${target})
    add_dependencies(tests-local ${target})
endforeach ()

option(FORCE_SANITIZER_ERRORS
        "If set, enable tests that force errors detected by the sanitizers."
        "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(bigtable_client_force_sanitizer_failures_test
            PRIVATE -DBIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

option(FORCE_STATIC_ANALYZER_ERRORS
        "If set, enable tests that force errors detected by the static analyzer."
        "")
if (FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(bigtable_client_force_sanitizer_failures_test
            PRIVATE -DBIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif (FORCE_STATIC_ANALYZER_ERRORS)

install(TARGETS bigtable_client
    EXPORT bigtable-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/bigtable/client
        FILES_MATCHING PATTERN "*.h"
        PATTERN "testing/*" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
    DESTINATION include/bigtable/client)

# Install the pkg-config files.
configure_file("config.pc.in" "bigtable_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Install the CMake configuration files.
configure_file("config.cmake.in" "bigtable_client-config.cmake" @ONLY)
configure_file("config-version.cmake.in"
    "bigtable_client-config-version.cmake" @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/bigtable_client-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
