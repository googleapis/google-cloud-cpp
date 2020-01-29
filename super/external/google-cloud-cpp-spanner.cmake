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

include(ExternalProjectHelper)
include(external/google-cloud-cpp-common)
include(external/googleapis)

if (NOT TARGET google-cloud-cpp-spanner-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_SPANNER_URL
        "https://github.com/googleapis/google-cloud-cpp-spanner/archive/v0.5.0.tar.gz"
    )
    set(GOOGLE_CLOUD_CPP_SPANNER_SHA256
        "06673c29657adeb4346b5c08b44739c2455bbc39a8be39c1ca9457ae5ec5f1cc")

    set_external_project_vars()
    ExternalProject_Add(
        google-cloud-cpp-spanner-project
        DEPENDS googleapis-project google-cloud-cpp-common-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/google-cloud-cpp-spanner"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_SPANNER_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_SPANNER_SHA256}
                 LIST_SEPARATOR
                 |
        CMAKE_ARGS -DBUILD_TESTING=OFF
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE OFF
        LOG_BUILD OFF
        LOG_INSTALL OFF)
endif ()
