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
include(external/curl)
include(external/crc32c)
include(external/grpc)
include(external/googleapis)
find_package(CURL REQUIRED)

if (NOT TARGET google-cloud-cpp-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_URL
        "https://github.com/googleapis/google-cloud-cpp/archive/v0.17.0.tar.gz")
    set(GOOGLE_CLOUD_CPP_SHA256
        "d67fed328d82aa404c3ab8f52814914f419a673573e3bbd98b4e6c405ca3cd06")

    set_external_project_vars()
    ExternalProject_Add(
        google-cloud-cpp-project
        DEPENDS googleapis-project grpc-project crc32c-project
                google-cloud-cpp-common-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/google-cloud-cpp"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_SHA256}
                 LIST_SEPARATOR
                 |
        CMAKE_ARGS -DBUILD_TESTING=OFF
                   -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE OFF
        LOG_BUILD OFF
        LOG_INSTALL OFF)
endif ()
