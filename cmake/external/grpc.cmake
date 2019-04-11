# ~~~
# Copyright 2018 Google LLC
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

include(ExternalProjectHelper)
include(external/c-ares)
include(external/ssl)
include(external/protobuf)

if (NOT TARGET gprc_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_GRPC_URL
        "https://github.com/grpc/grpc/archive/v1.19.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_GRPC_SHA256
        "f869c648090e8bddaa1260a271b1089caccbe735bf47ac9cd7d44d35a02fb129")

    set_external_project_build_parallel_level(PARALLEL)

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH "<INSTALL_DIR>/lib;<INSTALL_DIR>/lib64")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_INSTALL_RPATH
                   "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}")

    create_external_project_library_byproduct_list(grpc_byproducts
                                                   "grpc"
                                                   "grpc++"
                                                   "gpr"
                                                   "address_sorting")
    include(ExternalProject)
    externalproject_add(
        grpc_project
        DEPENDS c_ares_project protobuf_project ssl_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/grpc"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_GRPC_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GRPC_SHA256}
        LIST_SEPARATOR |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DgRPC_BUILD_TESTS=OFF
                   -DgRPC_ZLIB_PROVIDER=package
                   -DgRPC_SSL_PROVIDER=package
                   -DgRPC_CARES_PROVIDER=package
                   -DgRPC_PROTOBUF_PROVIDER=package
                   $<$<BOOL:${GOOGLE_CLOUD_CPP_USE_LIBCXX}>:
                   -DCMAKE_CXX_FLAGS=-stdlib=libc++
                   -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-lc++abi
                   >
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS
            ${grpc_byproducts}
            <INSTALL_DIR>/bin/grpc_cpp_plugin${CMAKE_EXECUTABLE_SUFFIX}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies grpc_project)
    endif ()

    add_library(gRPC::address_sorting INTERFACE IMPORTED)
    set_library_properties_for_external_project(gRPC::address_sorting
                                                address_sorting)
    add_dependencies(gRPC::address_sorting grpc_project)

    add_library(gRPC::gpr INTERFACE IMPORTED)
    set_library_properties_for_external_project(gRPC::gpr gpr)
    add_dependencies(gRPC::gpr grpc_project)
    set_property(TARGET gRPC::gpr
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES c-ares::cares)

    add_library(gRPC::grpc INTERFACE IMPORTED)
    set_library_properties_for_external_project(gRPC::grpc grpc)
    add_dependencies(gRPC::grpc grpc_project)
    set_property(TARGET gRPC::grpc
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          gRPC::address_sorting
                          gRPC::gpr
                          OpenSSL::SSL
                          OpenSSL::Crypto
                          protobuf::libprotobuf)

    add_library(gRPC::grpc++ INTERFACE IMPORTED)
    set_library_properties_for_external_project(gRPC::grpc++ grpc++)
    add_dependencies(gRPC::grpc++ grpc_project)
    set_property(TARGET gRPC::grpc++
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES gRPC::grpc c-ares::cares)

    # Discover the protobuf compiler and the gRPC plugin.
    add_executable(protoc IMPORTED)
    add_dependencies(protoc protobuf_project)
    set_executable_name_for_external_project(protoc protoc)

    add_executable(gRPC::grpc_cpp_plugin IMPORTED)
    add_dependencies(gRPC::grpc_cpp_plugin grpc_project)
    set_executable_name_for_external_project(gRPC::grpc_cpp_plugin
                                             grpc_cpp_plugin)

    list(APPEND PROTOBUF_IMPORT_DIRS "${CMAKE_BINARY_DIR}/external/include")
endif ()
