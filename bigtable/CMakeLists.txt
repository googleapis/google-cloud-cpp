# Copyright 2017 Google Inc.
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

# This should be included from the top level CMakeLists file

# Configure the Compiler options, we will be using C++11 features.
if (NOT WIN32)
    message("Setting CXX Standard")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()


# Turn on all available warnings and turn warnings into errors to
# stop the build if any warning is emitted.  We do this in each
# subdirectory because setting this for the top-level CMakeLists
# breaks submodules (such as gRPC) that do not conform to such an
# strict setting.
include(CheckCXXCompilerFlag)
if (NOT MSVC)
    CHECK_CXX_COMPILER_FLAG(-Werror COMPILER_SUPPORTS_WERROR)
    if (COMPILER_SUPPORTS_WERROR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif ()
    CHECK_CXX_COMPILER_FLAG(-Wall COMPILER_SUPPORTS_WALL)
    if (COMPILER_SUPPORTS_WALL)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    endif ()
endif ()

# If possible, enable a code coverage build type.
include(${PROJECT_SOURCE_DIR}/cmake/EnableCoverage.cmake)

# Include the functions to enable Clang sanitizers.
include(${PROJECT_SOURCE_DIR}/cmake/EnableSanitizers.cmake)

# Include support for clang-tidy if available
include(${PROJECT_SOURCE_DIR}/cmake/EnableClangTidy.cmake)

# We use abseil.io .
set(BUILD_TESTING OFF)
include(${PROJECT_SOURCE_DIR}/cmake/IncludeAbseil.cmake)
if (ABSEIL_ROOT_DIR)
    include_directories(${ABSEIL_INCLUDE_DIRS})
endif (ABSEIL_ROOT_DIR)

# Configure the location of proto files, particulary the googleapis protos.
include_directories(${GRPCPP_INCLUDE_DIRS} ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
set(PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis" "${PROJECT_SOURCE_DIR}")
if(GRPC_ROOT_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${GRPC_ROOT_DIR}/third_party/protobuf/src")
    include_directories("${GRPC_ROOT_DIR}/third_party/protobuf/src")
endif(GRPC_ROOT_DIR)

# Include the functions to compile proto files.
include(${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake)
PROTOBUF_GENERATE_CPP(PROTO_SOURCES PROTO_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/common.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/instance.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/table.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/data.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/status.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/error_details.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/annotations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/auth.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/http.proto)
GRPC_GENERATE_CPP_MOCKS(GRPCPP_SOURCES GRPCPP_HDRS GRPC_MOCK_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto)
include_directories("${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

# Create a library with the generated files from the relevant protos.
add_library(bigtable_protos ${PROTO_SOURCES} ${PROTO_HDRS} ${GRPCPP_SOURCES} ${GRPCPP_HDRS})
target_link_libraries(bigtable_protos ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

# Enable unit tests
enable_testing()

# the client library
add_library(bigtable_client
    client/cell.h
    client/client_options.h
    client/client_options.cc
    client/data_client.h
    client/data_client.cc
    client/internal/bulk_mutator.h
    client/internal/bulk_mutator.cc
    client/internal/conjunction.h
    client/internal/readrowsparser.h
    client/internal/readrowsparser.cc
    client/filters.h
    client/filters.cc
    client/idempotent_mutation_policy.h
    client/idempotent_mutation_policy.cc
    client/mutations.h
    client/mutations.cc
    client/row.h
    client/rpc_backoff_policy.h
    client/rpc_backoff_policy.cc
    client/rpc_retry_policy.h
    client/rpc_retry_policy.cc
    client/table.h
    client/table.cc
    client/version.h)
target_link_libraries(bigtable_client
    bigtable_protos absl::strings
    ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

add_library(bigtable_client_testing
    client/testing/chrono_literals.h
    client/testing/mock_data_client.h
    client/testing/table_test_fixture.h
    client/testing/table_test_fixture.cc)
target_link_libraries(bigtable_client_testing
    bigtable_client bigtable_protos absl::strings gmock
    ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

# the admin library
add_library(bigtable_admin_client
    admin/admin_client.h
    admin/admin_client.cc
    admin/column_family.h
    admin/table_admin.h
    admin/table_admin.cc
    admin/table_config.h
    admin/table_config.cc)
target_link_libraries(bigtable_admin_client bigtable_client bigtable_protos
    absl::strings ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

option(BIGTABLE_CLIENT_CLANG_TIDY
    "If set compiles the Cloud Bigtable client with clang-tidy."
    "")
if (CLANG_TIDY_EXE AND BIGTABLE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
        bigtable_client PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
    set_target_properties(
        bigtable_admin_client PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

# Define custom targets to simplify the scan-build scripts.
add_custom_target(depends-local)
add_dependencies(depends-local absl::strings gmock bigtable_protos)

# All tests get added to this target below.
add_custom_target(tests-local)

# List the unit tests, then setup the targets and dependencies.
set(bigtable_client_unit_tests
    client/cell_test.cc
    client/client_options_test.cc
    client/filters_test.cc
    client/force_sanitizer_failures_test.cc
    client/idempotent_mutation_policy_test.cc
    client/internal/bulk_mutator_test.cc
    client/internal/readrowsparser_test.cc
    client/mutations_test.cc
    client/table_apply_test.cc
    client/table_bulk_apply_test.cc
    client/table_test.cc
    client/row_test.cc
    client/rpc_backoff_policy_test.cc
    client/rpc_retry_policy_test.cc)
foreach (fname ${bigtable_client_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname})
    get_target_property(tname ${target} NAME)
    target_link_libraries(${target}
        bigtable_client_testing bigtable_client bigtable_protos
        gmock absl::strings ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES}
        ${PROTOBUF_LIBRARIES})
    add_test(NAME ${tname} COMMAND ${target})
    get_target_property(sources ${target} SOURCES)
    add_dependencies(tests-local ${target})
endforeach ()

# List the unit tests, then setup the targets and dependencies.
set(bigtable_admin_unit_tests
    admin/admin_client_test.cc
    admin/column_family_test.cc
    admin/table_admin_test.cc
    admin/table_config_test.cc)
foreach (fname ${bigtable_admin_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname})
    get_target_property(tname ${target} NAME)
    target_link_libraries(${target}
        bigtable_client_testing bigtable_admin_client bigtable_client
        bigtable_protos gmock absl::strings
        ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
    add_test(NAME ${tname} COMMAND ${target})
    get_target_property(sources ${target} SOURCES)
    add_dependencies(tests-local ${target})
endforeach ()

# Create a single executable that rolls up all the tests, this is convenient
# for CLion and other IDEs.
add_executable(bigtable_client_all_tests
    ${bigtable_client_unit_tests} ${bigtable_admin_unit_tests})
target_link_libraries(bigtable_client_all_tests
    bigtable_client_testing bigtable_admin_client bigtable_client
    bigtable_protos gmock absl::strings
    ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
add_dependencies(tests-local bigtable_client_all_tests)

option(FORCE_SANITIZER_ERRORS
    "If set, enable tests that force errors detected by the sanitizers."
    "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(client_force_sanitizer_failures_test
            PRIVATE -DBIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

option(FORCE_STATIC_ANALYZER_ERRORS
    "If set, enable tests that force errors detected by the static analyzer."
    "")
if (FORCE_STATIC_ANALYZER_ERRORS)
    target_compile_definitions(client_force_sanitizer_failures_test
        PRIVATE -DBIGTABLE_CLIENT_FORCE_STATIC_ANALYZER_ERRORS)
endif (FORCE_STATIC_ANALYZER_ERRORS)

# The integration tests, these are simply programs that connect to the
# Cloud Bigtable emulator.
add_executable(data_integration_test tests/data_integration_test.cc)
target_link_libraries(data_integration_test
    bigtable_admin_client bigtable_client bigtable_protos absl::strings
    ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
add_dependencies(tests-local data_integration_test)

add_executable(admin_integration_test tests/admin_integration_test.cc)
target_link_libraries(admin_integration_test
    bigtable_admin_client bigtable_client bigtable_protos absl::strings
    ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
add_dependencies(tests-local admin_integration_test)

# Define the install target
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if ("${LIB64}" STREQUAL "TRUE")
    set(LIBSUFFIX 64)
else()
    set(LIBSUFFIX "")
endif()
set(INSTALL_LIB_DIR lib${LIBSUFFIX} CACHE PATH "Installation directory for libraries")
install(TARGETS bigtable_admin_client bigtable_client bigtable_protos
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR})
install(DIRECTORY client/ DESTINATION include/bigtable/client FILES_MATCHING PATTERN "*.h")
install(DIRECTORY admin/ DESTINATION include/bigtable/admin FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ../third_party/abseil/absl DESTINATION include/bigtable FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ../third_party/abseil/absl DESTINATION include/bigtable FILES_MATCHING PATTERN "*.inc")
install(DIRECTORY ${CMAKE_BINARY_DIR}/bigtable/google/ DESTINATION include/bigtable/google FILES_MATCHING PATTERN "*.h")
