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

if (NOT TARGET zlib-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_ZLIB_URL
        "https://github.com/madler/zlib/archive/v1.2.11.tar.gz")
    set(GOOGLE_CLOUD_CPP_ZLIB_SHA256
        "629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff")

    set_external_project_build_parallel_level(PARALLEL)
    set_external_project_vars()

    include(ExternalProject)
    ExternalProject_Add(
        zlib-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/zlib"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_ZLIB_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_ZLIB_SHA256}
                 LIST_SEPARATOR
                 |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS}
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
