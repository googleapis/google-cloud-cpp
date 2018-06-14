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

include(external/c-ares)
include(external/protobuf)

if (NOT TARGET gprc_project)
    if("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles" OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        include(ProcessorCount)
        ProcessorCount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif()

    include(ExternalProject)
    ExternalProject_Add(grpc_project
        DEPENDS c_ares_project protobuf_project
        EXCLUDE_FROM_ALL ON
        PREFIX "external/gprc"
        INSTALL_DIR "external"
        URL https://github.com/grpc/grpc/archive/v1.10.0.tar.gz
        URL_HASH SHA256=39a73de6fa2a03bdb9c43c89a4283e09880833b3c1976ef3ce3edf45c8cacf72
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND}
            $ENV{CMAKE_FLAGS}
            ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
            -DgRPC_BUILD_TESTS=OFF
            -DgRPC_ZLIB_PROVIDER=package
            -DgRPC_SSL_PROVIDER=package
            -DgRPC_CARES_PROVIDER=package
            -DgRPC_PROTOBUF_PROVIDER=package
            -H<SOURCE_DIR>
            -B<BINARY_DIR>/Release
        BUILD_COMMAND
            ${CMAKE_COMMAND} --build Release ${PARALLEL}
        INSTALL_COMMAND
            ${CMAKE_COMMAND} --build Release --target install
            #        LOG_DOWNLOAD ON
            #        LOG_CONFIGURE ON
            #        LOG_BUILD ON
            #        LOG_INSTALL ON
            USES_TERMINAL_DOWNLOAD 1
            USES_TERMINAL_CONFIGURE 1
            USES_TERMINAL_BUILD 1
            USES_TERMINAL_INSTALL 1)
endif ()