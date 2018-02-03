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

cmake_minimum_required(VERSION 3.5)

# Define the project name, version, and main programming language.
set(PACKAGE_NAME      "google-cloud-cpp")
set(PACKAGE_VERSION   "0.1")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues")
project(${PACKAGE_NAME} CXX C)

set(PROJECT_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")

# WARNING: do not change any compiler flags or variables in this
# CMakeList file.  The grpc subdirectory (and its children), have
# carefully crafted flags, and you may break them by using your own
# settings.  In particular -Werror -Wall breaks protobuf or grpc in many
# platforms, for an example of these problems (but not the only one) see:
#    https://github.com/grpc/grpc/pull/11376

# Configure the grpc library.
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "module" CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER PROPERTY STRINGS "module" "package")

if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "module")
    if (NOT GRPC_ROOT_DIR)
        set(GRPC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/grpc)
    endif ()
    if (NOT EXISTS "${GRPC_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "GOOGLE_CLOUD_CPP_GRPC_PROVIDER is \"module\" but GRPC_ROOT_DIR is wrong")
    endif ()
    add_subdirectory(${GRPC_ROOT_DIR} third_party/grpc EXCLUDE_FROM_ALL)
    set(GRPCPP_LIBRARIES grpc++)
    set(GRPC_LIBRARIES grpc)
    set(PROTOBUF_LIBRARIES libprotobuf)
    set(GRPC_BINDIR "${PROJECT_BINARY_DIR}/third_party/grpc")
    set(GRPC_INCLUDE_DIRS ${GRPC_ROOT_DIR}/include)
    set(GRPCPP_INCLUDE_DIRS ${GRPC_ROOT_DIR}/include)
    set(PROTOBUF_INCLUDE_DIRS ${GRPC_ROOT_DIR}/third_party/protobuf/include)
    set(PROTOBUF_PROTOC_EXECUTABLE "${GRPC_BINDIR}/third_party/protobuf/protoc")
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    set(PROTOC_GRPCPP_PLUGIN_EXECUTABLE "${GRPC_BINDIR}/grpc_cpp_plugin")
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package")
    # ... find the grpc and grpc++ libraries ...
    if (WIN32)
        # ... use find_package and vcpkg on Windows ...
        find_package(GRPC REQUIRED grpc>=1.4)
        find_package(PROTOBUF REQUIRED protobuf>=3.0)
        find_path(GTEST_INCLUDE_DIR googletest)
        find_library(GTEST_LIBRARY googletest)
        link_directories(${GRPC_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
        set(GRPCPP_LIBRARIES gRPC::grpc++)
        set(GRPC_LIBRARIES gRPC::grpc)
        set(PROTOBUF_LIBRARIES protobuf::libprotobuf)
        # To avoid macro conflict on windows.
        add_compile_options(/wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
        # Use the same settings that gRPC uses...
        add_definitions(-D_WIN32_WINNT=0x600 -D_SCL_SECURE_NO_WARNINGS)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
        if (MSVC)
            add_definitions(/wd4065 /wd4506 /wd4267 /wd4800 /wd4291 /wd4838)
            if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
                set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
                set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
            endif ()
        endif ()
    else ()
        # ... use pkg-config on Linux and Mac OSX ...
        include(FindPkgConfig)
        pkg_check_modules(GRPCPP REQUIRED grpc++>=1.4.1)
        pkg_check_modules(GRPC REQUIRED grpc>=4.0)
        pkg_check_modules(PROTOBUF REQUIRED protobuf>=3.4)
        link_directories(${GRPCPP_LIBRARY_DIRS} ${GRPC_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
    endif ()
    # ... discover protoc and friends ...
    include(FindProtobuf)
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

# Enable testing in this directory so we can do a top-level `make test`.
enable_testing()

# Add subprojects here.
add_subdirectory(bigtable)

add_subdirectory(tests/grpc-crash)
