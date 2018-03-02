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
find_package(Threads REQUIRED)


# Configure the gRPC dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER "module"
        CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER
        PROPERTY STRINGS "module" "package" "vcpkg" "pkg-config")

# Additional compile-time definitions for WIN32.  We need to manually set these
# because Protobuf / gRPC do not (always) set them.
set(GOOGLE_CLOUD_CPP_WIN32_DEFINITIONS
        _WIN32_WINNT=0x600 _SCL_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
# While the previous definitions are applicable to all compilers on Windows, the
# following options are specific to MSVC, they would not apply to MinGW:
set(GOOGLE_CLOUD_CPP_MSVC_COMPILE_OPTIONS
        /wd4005 /wd4065 /wd4068 /wd4146 /wd4244 /wd4267 /wd4291 /wd4506
        /wd4800 /wd4838 /wd4996)

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

    # The necessary compiler options and definitions are not defined by the
    # targets, we need to add them.
    if (WIN32)
        target_compile_definitions(libprotobuf PUBLIC ${GOOGLE_CLOUD_CPP_WIN32_DEFINITIONS})
    endif (WIN32)
    if (MSVC)
        target_compile_options(libprotobuf PUBLIC ${GOOGLE_CLOUD_CPP_MSVC_COMPILE_OPTIONS})
    endif (MSVC)

    # TODO(#286) - workaround build breakage for gRPC v1.10.x on Ubuntu:16.04.
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG(-Wno-maybe-uninitialized __cxx_supports_wno_maybe_unitialized)
    if (__cxx_supports_wno_maybe_unitialized)
        target_compile_options(ssl PRIVATE -Wno-maybe-uninitialized)
    endif ()

    # The binary name is different on some platforms, use CMake magic to get it.
    set(PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    set(PROTOC_GRPCPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package"
        OR "${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "vcpkg")
    find_package(GRPC REQUIRED grpc>=1.9)
    find_package(PROTOBUF REQUIRED protobuf>=3.5)

    if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
        message(STATUS " RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
        message(STATUS " DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        message(STATUS " RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
        message(STATUS " DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
        message(STATUS " DEFAULT=${CMAKE_CXX_FLAGS}")
    endif ()

    # The necessary compiler options and definitions are not defined by the
    # targets, we need to add them.
    if (WIN32)
        set_property(TARGET protobuf::libprotobuf APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS ${GOOGLE_CLOUD_CPP_WIN32_DEFINITIONS})
    endif (WIN32)
    if (MSVC)
        set_property(TARGET protobuf::libprotobuf APPEND PROPERTY
                INTERFACE_COMPILE_OPTIONS ${GOOGLE_CLOUD_CPP_MSVC_COMPILE_OPTIONS})
    endif (MSVC)

    # Discover the protobuf compiler and the gRPC plugin.
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
elseif ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "pkg-config")
    # Use pkg-config to find the libraries.
    find_package(PkgConfig REQUIRED)
    # We need a helper function to convert pkg-config(1) output into target
    # properties.
    include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake)

    pkg_check_modules(Protobuf REQUIRED protobuf>=3.5)
    add_library(protobuf::libprotobuf INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(protobuf::libprotobuf Protobuf)
    set_property(TARGET protobuf::libprotobuf APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Threads::Threads)

    pkg_check_modules(gRPC REQUIRED grpc>=1.9)
    add_library(gRPC::grpc INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(gRPC::grpc gRPC)
    set_property(TARGET gRPC::grpc APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES protobuf::libprotobuf)

    pkg_check_modules(gRPC++ REQUIRED grpc++>=1.9)
    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(gRPC::grpc++ gRPC++)
    set_property(TARGET gRPC::grpc++ APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES gRPC::grpc)

    # Discover the protobuf compiler and the gRPC plugin.
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
