# ~~~
# Copyright 2019 Google LLC
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

find_package(google_cloud_cpp_common CONFIG REQUIRED)
find_package(google_cloud_cpp_grpc_utils CONFIG REQUIRED)

include(EnableClangTidy)
include(EnableWerror)
set(DOXYGEN_PROJECT_NAME "Google Cloud Spanner C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Spanner")
set(DOXYGEN_PROJECT_NUMBER "${SPANNER_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples)
include(EnableDoxygen)

configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(spanner_client
            backoff_policy.h
            client_options.cc
            client_options.h
            commit_result.h
            database_admin_client.cc
            database_admin_client.h
            date.h
            internal/base64.cc
            internal/base64.h
            internal/database_admin_retry.cc
            internal/database_admin_retry.h
            internal/database_admin_stub.cc
            internal/database_admin_stub.h
            internal/date.cc
            internal/date.h
            internal/merge_chunk.cc
            internal/merge_chunk.h
            internal/polling_loop.h
            internal/retry_loop.cc
            internal/retry_loop.h
            internal/spanner_stub.cc
            internal/spanner_stub.h
            internal/time.cc
            internal/time.h
            internal/time_format.cc
            internal/time_format.h
            internal/transaction_impl.cc
            internal/transaction_impl.h
            internal/tuple_utils.h
            mutations.cc
            mutations.h
            polling_policy.h
            result_set.h
            retry_policy.h
            row.h
            row_parser.h
            sql_partition.cc
            sql_partition.h
            sql_statement.cc
            sql_statement.h
            timestamp.h
            transaction.cc
            transaction.h
            value.cc
            value.h
            version.cc
            version.h
            version_info.h)
target_include_directories(spanner_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_link_libraries(spanner_client
                      PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
                             googleapis-c++::spanner_protos)
set_target_properties(spanner_client
                      PROPERTIES VERSION
                                 "${SPANNER_CLIENT_VERSION}"
                                 SOVERSION
                                 "${SPANNER_CLIENT_VERSION_MAJOR}")
target_compile_options(spanner_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_SPANNER_EXCEPTIONS_FLAG})

google_cloud_cpp_add_clang_tidy(spanner_client)
google_cloud_cpp_add_common_options(spanner_client)

add_library(googleapis-c++::spanner_client ALIAS spanner_client)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(spanner_client)

# Define the tests in a function so we have a new scope for variable names.
function (spanner_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    find_package(google_cloud_cpp_testing CONFIG REQUIRED)

    set(spanner_client_unit_tests
        client_options_test.cc
        database_admin_client_test.cc
        date_test.cc
        internal/base64_test.cc
        internal/date_test.cc
        internal/merge_chunk_test.cc
        internal/polling_loop_test.cc
        internal/retry_loop_test.cc
        internal/spanner_stub_test.cc
        internal/time_format_test.cc
        internal/time_test.cc
        internal/transaction_impl_test.cc
        internal/tuple_utils_test.cc
        mutations_test.cc
        result_set_test.cc
        retry_policy_test.cc
        row_test.cc
        row_parser_test.cc
        sql_partition_test.cc
        spanner_version_test.cc
        sql_statement_test.cc
        transaction_test.cc
        value_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_client_unit_tests.bzl"
                         "spanner_client_unit_tests")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(spanner-client-tests)

    # Generate a target for each unit test.
    foreach (fname ${spanner_client_unit_tests})
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
                              PRIVATE googleapis-c++::spanner_client
                                      google_cloud_cpp_testing
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest)
        google_cloud_cpp_add_clang_tidy(${target})
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(spanner-client-tests ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    spanner_client_define_tests()
endif (BUILD_TESTING)

add_subdirectory(integration_tests)

# Only compile the samples if we're building with exceptions enabled. They
# require exceptions to keep them simple and idiomatic.
if (GOOGLE_CLOUD_CPP_SPANNER_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(samples)
endif (GOOGLE_CLOUD_CPP_SPANNER_ENABLE_CXX_EXCEPTIONS)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT spanner-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/spanner_client")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS spanner_client
        EXPORT spanner-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
        DESTINATION include/google/cloud/spanner
        FILES_MATCHING
        PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
        DESTINATION include/google/cloud/spanner
        FILES_MATCHING
        PATTERN "*.inc")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_SPANNER_CONFIG_VERSION_MAJOR ${SPANNER_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_SPANNER_CONFIG_VERSION_MINOR ${SPANNER_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_SPANNER_CONFIG_VERSION_PATCH ${SPANNER_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_SPANNER_PC_NAME "The Google Cloud Spanner C++ Client Library")
set(GOOGLE_CLOUD_SPANNER_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Spanner.")
set(GOOGLE_CLOUD_SPANNER_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common")
set(GOOGLE_CLOUD_SPANNER_PC_LIBS "-lspanner_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/spanner/config.pc.in"
               "spanner_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/spanner_client.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "spanner_client-config.cmake" @ONLY)
configure_file(
    "${PROJECT_SOURCE_DIR}/google/cloud/spanner/config-version.cmake.in"
    "spanner_client-config-version.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/spanner_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/spanner_client-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/spanner_client")
