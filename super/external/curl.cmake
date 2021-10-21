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

include(external/c-ares)
include(external/ssl)
include(external/zlib)

if (NOT TARGET curl-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_CURL_URL
        "https://curl.haxx.se/download/curl-7.69.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_CURL_SHA256
        "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98")

    set_external_project_build_parallel_level(PARALLEL)
    set_external_project_vars()

    include(ExternalProject)
    ExternalProject_Add(
        curl-project
        DEPENDS c-ares-project ssl-project zlib-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/curl"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_CURL_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_CURL_SHA256}
        LIST_SEPARATOR |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS}
                   -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   # libcurl automatically enables a number of protocols. With
                   # static libraries this is a problem. The indirect
                   # dependencies, such as libldap, become hard to predict and
                   # manage. Setting HTTP_ONLY=ON disables all optional
                   # protocols and meets our needs. If the application needs a
                   # version of libcurl with other protocols enabled they can
                   # compile against it by using find_package() and defining the
                   # `curl_project` target.
                   -DHTTP_ONLY=ON
                   -DCMAKE_ENABLE_OPENSSL=ON
                   -DENABLE_ARES=ON
                   -DCMAKE_DEBUG_POSTFIX=
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${PARALLEL}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
