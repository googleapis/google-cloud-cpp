# ~~~
# Copyright 2020 Google LLC
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

if (NOT TARGET re2-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_RE2_URL
        "https://github.com/google/re2/archive/2020-11-01.tar.gz")
    set(GOOGLE_CLOUD_CPP_RE2_SHA256
        "8903cc66c9d34c72e2bc91722288ebc7e3ec37787ecfef44d204b2d6281954d7")

    set_external_project_build_parallel_level(PARALLEL)
    set_external_project_vars()

    include(ExternalProject)
    ExternalProject_Add(
        re2-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/re2"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_RE2_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_RE2_SHA256}
        LIST_SEPARATOR |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS}
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                   -DRE2_BUILD_TESTING=OFF
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
