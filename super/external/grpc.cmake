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

if (NOT TARGET grpc-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_GRPC_URL
        "https://github.com/grpc/grpc/archive/v1.24.3.tar.gz")
    set(GOOGLE_CLOUD_CPP_GRPC_SHA256
        "c84b3fa140fcd6cce79b3f9de6357c5733a0071e04ca4e65ba5f8d306f10f033")

    set_external_project_build_parallel_level(PARALLEL)
    set_external_project_vars()

    include(ExternalProject)
    ExternalProject_Add(
        grpc-project
        DEPENDS c-ares-project protobuf-project ssl-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/grpc"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_GRPC_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GRPC_SHA256}
                 LIST_SEPARATOR
                 |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS}
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   -DgRPC_BUILD_TESTS=OFF
                   -DgRPC_ZLIB_PROVIDER=package
                   -DgRPC_SSL_PROVIDER=package
                   -DgRPC_CARES_PROVIDER=package
                   -DgRPC_PROTOBUF_PROVIDER=package
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
