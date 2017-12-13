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
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# For the GCC and Clang compiler families enable a Coverage build type.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
        "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # But only if the compiler supports the --coverage flag.  Older versions
    # of these compilers did not support it.
    set(OLD_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} --coverage")
    check_cxx_compiler_flag("--coverage" CXX_SUPPORTS_COVERAGE_FLAG)
    set(CMAKE_REQUIRED_FLAGS "${OLD_CMAKE_REQUIRED_FLAGS}")
    if(CXX_SUPPORTS_COVERAGE_FLAG)
        # Coverage build type
        set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage"
                CACHE STRING
                "Flags used by the C++ compiler during coverage builds."
                FORCE)
        # A bit of a hack, we should not assume the C compiler also supports
        # --coverage
        set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} --coverage"
                CACHE STRING
                "Flags used by the C compiler during coverage builds."
                FORCE)
        set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
                "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage" CACHE STRING
                "Flags used for linking binaries during coverage builds."
                FORCE)
        set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
                "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage" CACHE STRING
                "Flags used by the shared libraries linker during coverage builds."
                FORCE)
        mark_as_advanced(
                CMAKE_CXX_FLAGS_COVERAGE
                CMAKE_C_FLAGS_COVERAGE
                CMAKE_EXE_LINKER_FLAGS_COVERAGE
                CMAKE_SHARED_LINKER_FLAGS_COVERAGE)
        set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
                "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Coverage."
                FORCE)
    endif(CXX_SUPPORTS_COVERAGE_FLAG)
endif()

# Include the functions to enable Clang sanitizers.
include(${PROJECT_SOURCE_DIR}/cmake/EnableSanitizers.cmake)

# Include support for clang-tidy if available
include(${PROJECT_SOURCE_DIR}/cmake/EnableClangTidy.cmake)

# We use abseil.io .
include_directories(${PROJECT_THIRD_PARTY_DIR}/abseil)

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
add_library(googleapis ${PROTO_SOURCES} ${PROTO_HDRS} ${GRPCPP_SOURCES} ${GRPCPP_HDRS})
target_link_libraries(googleapis ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

# Enable unit tests
enable_testing()

# the client library
add_library(bigtable_data_api
        client/cell.h
        client/chrono_literals.h
        client/client_options.h
        client/client_options.cc
        client/data.h
        client/data.cc
        client/filters.h
        client/filters.cc
        client/idempotent_mutation_policy.h
        client/idempotent_mutation_policy.cc
        client/detail/bulk_mutator.h
        client/detail/bulk_mutator.cc
        client/mutations.h
        client/mutations.cc
        client/rpc_backoff_policy.h
        client/rpc_backoff_policy.cc
        client/rpc_retry_policy.h
        client/rpc_retry_policy.cc
        client/version.h)
target_link_libraries(bigtable_data_api googleapis ${GRPCPP_LIBRARIES}
  ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

option(BIGTABLE_CLIENT_CLANG_TIDY
    "If set compiles the Cloud Bigtable client with clang-tidy."
    "")
if (CLANG_TIDY_EXE AND BIGTABLE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
        bigtable_data_api PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

# List the unit tests, then setup the targets and dependencies.
set(all_unit_tests
        client/cell_test.cc
        client/client_options_test.cc
        client/filters_test.cc
        client/force_sanitizer_failures_test.cc
        client/idempotent_mutation_policy_test.cc
        client/detail/bulk_mutator_test.cc
        client/mutations_test.cc
        client/table_apply_test.cc
        client/table_bulk_apply_test.cc
        client/rpc_backoff_policy_test.cc
        client/rpc_retry_policy_test.cc)
foreach (fname ${all_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    add_executable(${target} ${fname}
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/src/gmock-all.cc)
    get_target_property(tname ${target} NAME)
    target_include_directories(${target}
            PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
            PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest
            PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include
            PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock)
    target_link_libraries(${target} bigtable_data_api googleapis
            ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
    add_test(NAME ${tname} COMMAND ${target})
    get_target_property(sources ${target} SOURCES)
endforeach()

# Create a single executable that rolls up all the tests, this is convenient
# for CLion and other IDEs.
add_executable(bigtable_client_all_tests ${all_unit_tests}
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/src/gmock-all.cc)
target_include_directories(bigtable_client_all_tests
        PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
        PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest
        PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include
        PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock)
target_link_libraries(bigtable_client_all_tests bigtable_data_api googleapis
        ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

option(FORCE_SANITIZER_ERRORS
        "If set enable tests that force errors detected by the sanitizers."
        "")
if (FORCE_SANITIZER_ERRORS)
    target_compile_definitions(client_force_sanitizer_failures_test
            PRIVATE -DBIGTABLE_CLIENT_FORCE_SANITIZER_ERRORS)
endif (FORCE_SANITIZER_ERRORS)

# The integration tests, these are simply programs that connect to the
# Cloud Bigtable emulator.
add_executable(apply_test tests/apply_test.cc)
target_link_libraries(apply_test bigtable_data_api googleapis
        ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
