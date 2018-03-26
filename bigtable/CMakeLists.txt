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
set(BIGTABLE_CLIENT_VERSION_MAJOR 0)
set(BIGTABLE_CLIENT_VERSION_MINOR 1)
set(BIGTABLE_CLIENT_VERSION_PATCH 0)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE bigtable_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(bigtable_common_options INTERFACE)

if (MSVC)
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
    if (_cpp_latest_flag_supported)
        target_compile_options(bigtable_common_options INTERFACE "/std:c++latest")
    endif ()
endif ()

# Find out what flags turn on all available warnings and turn those warnings
# into errors.
include(CheckCXXCompilerFlag)
if (NOT MSVC)
    CHECK_CXX_COMPILER_FLAG(-Wall COMPILER_SUPPORTS_WALL)
    if (COMPILER_SUPPORTS_WALL)
        target_compile_options(bigtable_common_options INTERFACE "-Wall")
    endif ()
    CHECK_CXX_COMPILER_FLAG(-Werror COMPILER_SUPPORTS_WERROR)
    if (COMPILER_SUPPORTS_WERROR)
        target_compile_options(bigtable_common_options INTERFACE "-Werror")
    endif ()
endif ()

# If possible, enable a code coverage build type.
include(${PROJECT_SOURCE_DIR}/cmake/EnableCoverage.cmake)

# Include the functions to enable Clang sanitizers.
include(${PROJECT_SOURCE_DIR}/cmake/EnableSanitizers.cmake)

# Include support for clang-tidy if available
include(${PROJECT_SOURCE_DIR}/cmake/EnableClangTidy.cmake)

# C++ Exceptions are enabled by default, but allow the user to turn them off.
include(${PROJECT_SOURCE_DIR}/cmake/EnableCxxExceptions.cmake)

# Configure the location of proto files, particulary the googleapis protos.
set(PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis" "${PROJECT_SOURCE_DIR}")
if(GRPC_ROOT_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${GRPC_ROOT_DIR}/third_party/protobuf/src")
endif(GRPC_ROOT_DIR)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

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

# Create a library with the generated files from the relevant protos.
add_library(bigtable_protos ${PROTO_SOURCES} ${PROTO_HDRS} ${GRPCPP_SOURCES} ${GRPCPP_HDRS})
target_link_libraries(bigtable_protos gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
target_include_directories(bigtable_protos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_protos PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(bigtable::protos ALIAS bigtable_protos)

# Enable unit tests
enable_testing()

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
    execute_process(COMMAND git rev-parse --short HEAD
            OUTPUT_VARIABLE GIT_HEAD_LOG ERROR_VARIABLE GIT_HEAD_LOG)
else ()
    set(GIT_HEAD_LOG "(unknown-git-commit)")
endif ()
string(REPLACE "\n" "" GIT_HEAD ${GIT_HEAD_LOG})
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
configure_file(client/build_info.cc.in client/build_info.cc)

include(${PROJECT_SOURCE_DIR}/cmake/IncludeGMock.cmake)

if (${CMAKE_VERSION} VERSION_LESS "3.9")
    # Old versions of CMake have really poor support for Doxygen generation.
    message(STATUS "Doxygen generation only enabled for cmake 3.9 and higher")
else ()
    find_package(Doxygen)
    if (Doxygen_FOUND)
        set(DOXYGEN_PROJECT_NAME "Google Cloud Bigtable C++ Client")
        set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Bigtable")
        set(DOXYGEN_PROJECT_NUMBER
                "${BIGTABLE_CLIENT_VERSION_MAJOR}.${BIGTABLE_CLIENT_VERSION_MINOR}.${BIGTABLE_CLIENT_VERSION_PATCH}")
        set(DOXYGEN_EXAMPLE_PATH examples)
        set(DOXYGEN_PREDEFINED "BIGTABLE_CLIENT_NS=v${BIGTABLE_CLIENT_VERSION_MAJOR}")
        set(DOXYGEN_EXCLUDE_PATTERNS "*/client/internal/*;*/client/testing/*;*/*_test.cc")
        include(${PROJECT_SOURCE_DIR}/cmake/DoxygenCommon.cmake)

        doxygen_add_docs(bigtable-docs doc client
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bigtable
                COMMENT "Generate HTML documentation")
        add_dependencies(doxygen-docs bigtable-docs)
    endif ()
endif ()

# Define custom targets to simplify the scan-build static analysis scripts: we
# want to compile these targets without static analysis, as they are not part
# of this project, and we also do not want to list all the dependencies in the
# scripts and here.  So we create a target to build the dependencies, build that
# without static analysis, and then build "all" with static analysis.
add_custom_target(depends-local)
add_dependencies(depends-local gmock bigtable_protos)

# All tests get added to this target below.
add_custom_target(tests-local)
add_subdirectory(client)
add_subdirectory(tests)

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(benchmarks)
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

option(BIGTABLE_CLIENT_CLANG_TIDY
        "If set compiles the Cloud Bigtable client with clang-tidy."
        "")
if (CLANG_TIDY_EXE AND BIGTABLE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
            bigtable_client PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS bigtable_protos bigtable_common_options
    EXPORT bigtable-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install proto generated files into bigtable/google.
install(DIRECTORY ${CMAKE_BINARY_DIR}/bigtable/google/ DESTINATION include/google FILES_MATCHING PATTERN "*.h")

# The exports can only be installed if all the dependencies are installed.
# CMake needs to know where the submodules will be installed from,
if (NOT ${GOOGLE_CLOUD_CPP_GRPC_PROVIDER} STREQUAL "module")
    # Export the CMake targets to make it easy to create configuration files.
    install(EXPORT bigtable-targets DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
endif ()
