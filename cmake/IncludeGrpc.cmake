# ~~~
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
# ~~~

# gRPC always requires thread support.
find_package(Threads REQUIRED)

# Configure the gRPC dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_GRPC_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
    CACHE STRING "How to find the gRPC library")
set_property(CACHE GOOGLE_CLOUD_CPP_GRPC_PROVIDER
             PROPERTY STRINGS
                      "external"
                      "package"
                      "pkg-config")

# Additional compile-time definitions for WIN32.  We need to manually set these
# because Protobuf / gRPC do not (always) set them.
set(GOOGLE_CLOUD_CPP_WIN32_DEFINITIONS
    _WIN32_WINNT=0x600
    _SCL_SECURE_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS)
# While the previous definitions are applicable to all compilers on Windows, the
# following options are specific to MSVC, they would not apply to MinGW:
set(GOOGLE_CLOUD_CPP_MSVC_COMPILE_OPTIONS
    /wd4005
    /wd4065
    /wd4068
    /wd4146
    /wd4244
    /wd4267
    /wd4291
    /wd4506
    /wd4800
    /wd4838
    /wd4996)

if ("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "external")
    include(external/grpc)
elseif("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "package")
    find_package(protobuf REQUIRED protobuf>=3.5.2)
    find_package(gRPC REQUIRED gRPC>=1.9)

    if (NOT TARGET protobuf::libprotobuf)
        message(
            FATAL_ERROR
                "Expected protobuf::libprotobuf target created by FindProtobuf")
    endif ()

    if (VCPKG_TARGET_TRIPLET MATCHES "-static$" AND MSVC)
        # Replace the runtime flags because all the dependencies are linked
        # statically in this case.
        message(STATUS " RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
        message(STATUS " DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
        foreach (flag_var
                 CMAKE_CXX_FLAGS
                 CMAKE_CXX_FLAGS_DEBUG
                 CMAKE_CXX_FLAGS_RELEASE
                 CMAKE_CXX_FLAGS_MINSIZEREL
                 CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if (${flag_var} MATCHES "/MD")
                string(REGEX
                       REPLACE "/MD"
                               "/MT"
                               ${flag_var}
                               "${${flag_var}}")
            endif ()
        endforeach (flag_var)
        message(STATUS " RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
        message(STATUS " DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
        message(STATUS " DEFAULT=${CMAKE_CXX_FLAGS}")
    endif ()

    # The necessary compiler options and definitions are not defined by the
    # targets, we need to add them.
    if (WIN32)
        set_property(TARGET protobuf::libprotobuf
                     APPEND
                     PROPERTY INTERFACE_COMPILE_DEFINITIONS
                              ${GOOGLE_CLOUD_CPP_WIN32_DEFINITIONS})
    endif (WIN32)
    if (MSVC)
        set_property(TARGET protobuf::libprotobuf
                     APPEND
                     PROPERTY INTERFACE_COMPILE_OPTIONS
                              ${GOOGLE_CLOUD_CPP_MSVC_COMPILE_OPTIONS})
    endif (MSVC)

    # Discover the protobuf compiler and the gRPC plugin.
    find_program(
        PROTOBUF_PROTOC_EXECUTABLE
        NAMES protoc
        DOC "The Google Protocol Buffers Compiler"
        PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug)
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    add_executable(protoc IMPORTED)
    set_property(TARGET protoc
                 PROPERTY IMPORTED_LOCATION ${PROTOBUF_PROTOC_EXECUTABLE})

    find_program(
        PROTOC_GRPCPP_PLUGIN_EXECUTABLE
        NAMES grpc_cpp_plugin
        DOC "The Google Protocol Buffers Compiler"
        PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug)
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
    add_executable(grpc_cpp_plugin IMPORTED)
    set_property(TARGET grpc_cpp_plugin
                 PROPERTY IMPORTED_LOCATION ${PROTOC_GRPCPP_PLUGIN_EXECUTABLE})

    if ("${Protobuf_IMPORT_DIRS}" STREQUAL "")
        list(APPEND PROTOBUF_IMPORT_DIRS ${Protobuf_INCLUDE_DIRS})
    else()
        list(APPEND PROTOBUF_IMPORT_DIRS ${Protobuf_IMPORT_DIRS})
    endif ()

elseif("${GOOGLE_CLOUD_CPP_GRPC_PROVIDER}" STREQUAL "pkg-config")

    # Use pkg-config to find the libraries.
    find_package(PkgConfig REQUIRED)

    # We need a helper function to convert pkg-config(1) output into target
    # properties.
    include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake)

    pkg_check_modules(Protobuf REQUIRED protobuf>=3.5)
    add_library(protobuf::libprotobuf INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(protobuf::libprotobuf Protobuf)
    set_property(TARGET protobuf::libprotobuf
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
    list(APPEND PROTOBUF_IMPORT_DIRS ${Protobuf_INCLUDE_DIRS})

    pkg_check_modules(gRPC REQUIRED grpc>=1.9)
    add_library(gRPC::grpc INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(gRPC::grpc gRPC)
    set_property(TARGET gRPC::grpc
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES protobuf::libprotobuf)

    pkg_check_modules(gRPC++ REQUIRED grpc++>=1.9)
    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(gRPC::grpc++ gRPC++)
    set_property(TARGET gRPC::grpc++
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES gRPC::grpc)

    # Discover the protobuf compiler and the gRPC plugin.
    find_program(
        PROTOBUF_PROTOC_EXECUTABLE
        NAMES protoc
        DOC "The Google Protocol Buffers Compiler"
        PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug)
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    add_executable(protoc IMPORTED)
    set_property(TARGET protoc
                 PROPERTY IMPORTED_LOCATION ${PROTOBUF_PROTOC_EXECUTABLE})

    find_program(
        PROTOC_GRPCPP_PLUGIN_EXECUTABLE
        NAMES grpc_cpp_plugin
        DOC "The Google Protocol Buffers Compiler"
        PATHS
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
            ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug)
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
    add_executable(grpc_cpp_plugin IMPORTED)
    set_property(TARGET grpc_cpp_plugin
                 PROPERTY IMPORTED_LOCATION ${PROTOC_GRPCPP_PLUGIN_EXECUTABLE})

endif ()
