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

# gRPC always requires thread support.
include(FindThreads)

# Configure the gRPC dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "module"
        CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER
        PROPERTY STRINGS "module" "package" "pkg-config")

if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "module")
    if (NOT GRPC_ROOT_DIR)
        set(GRPC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/grpc)
    endif ()
    if (NOT EXISTS "${GRPC_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "GOOGLE_CLOUD_CPP_GRPC_PROVIDER is \"module\" but GRPC_ROOT_DIR is wrong")
    endif ()
    add_subdirectory(${GRPC_ROOT_DIR} third_party/grpc EXCLUDE_FROM_ALL)
    add_library(gRPC::grpc++ ALIAS grpc++)
    add_library(gRPC::grpc ALIAS grpc)
    add_library(protobuf::libprotobuf ALIAS libprotobuf)
    set(GRPC_BINDIR "${PROJECT_BINARY_DIR}/third_party/grpc")
    set(PROTOBUF_PROTOC_EXECUTABLE "${GRPC_BINDIR}/third_party/protobuf/protoc")
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    set(PROTOC_GRPCPP_PLUGIN_EXECUTABLE "${GRPC_BINDIR}/grpc_cpp_plugin")
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package"
        OR "${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "vcpkg")
    find_package(GRPC REQUIRED grpc>=1.8)
    find_package(PROTOBUF REQUIRED protobuf>=3.5)

    if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
        message(STATUS " RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
        message(STATUS " DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif ()
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "pkg-config")
    # Use pkg-config to find the libraries.
    include(FindPkgConfig)
    find_package(Protobuf 3.5 REQUIRED)

    pkg_check_modules(gRPC++ REQUIRED IMPORTED_TARGET grpc++>=1.8)
    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    set_property(TARGET gRPC::grpc++ PROPERTY INTERFACE_LINK_LIBRARIES
            PkgConfig::gRPC++)

    pkg_check_modules(gRPC REQUIRED IMPORTED_TARGET grpc)
    add_library(gRPC::grpc INTERFACE IMPORTED)
    set_property(TARGET gRPC::grpc PROPERTY INTERFACE_LINK_LIBRARIES
            PkgConfig::gRPC)

    # ... discover protoc and friends ...
    find_program(PROTOBUF_PROTOC_EXECUTABLE
            NAMES protoc
            DOC "The Google Protocol Buffers Compiler"
            PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug
            )
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    find_program(PROTOC_GRPCPP_PLUGIN_EXECUTABLE
            NAMES grpc_cpp_plugin
            DOC "The Google Protocol Buffers Compiler"
            PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug
            )
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
endif ()
