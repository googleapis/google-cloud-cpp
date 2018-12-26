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

if (NOT TARGET curl_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_CURL_URL
        "https://curl.haxx.se/download/curl-7.60.0.tar.gz")
    set(GOOGLE_CLOUD_CPP_CURL_SHA256
        "e9c37986337743f37fd14fe8737f246e97aec94b39d1b71e8a5973f72a9fc4f5")

    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

    create_external_project_library_byproduct_list(curl_byproducts "curl")

    include(ExternalProject)
    externalproject_add(
        curl_project
        DEPENDS c_ares_project ssl_project zlib_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/curl"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_CURL_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_CURL_SHA256}
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                   # libcurl automatically enables a number of protocols. With
                   # static libraries this is a problem. The indirect
                   # dependencies, such as libldap, become hard to predict and
                   # manage. Setting HTTP_ONLY=ON disables all optional
                   # protocols and meets our needs. If the application needs
                   # a version of libcurl with other protocols enabled they
                   # can select it using GOOGLE_CLOUD_CPP_CURL_PROVIDER=package.
                   -DHTTP_ONLY=ON
                   -DCMAKE_BUILD_TYPE=Release
                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                   -DENABLE_ARES=ON
                   -DCURL_STATICLIB=$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>
                   -DCMAKE_DEBUG_POSTFIX=
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${curl_byproducts}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    include(ExternalProjectHelper)
    add_library(CURL::CURL INTERFACE IMPORTED)
    add_dependencies(CURL::CURL curl_project)
    set_library_properties_for_external_project(CURL::CURL curl)
    set_property(TARGET CURL::CURL
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          c-ares::cares
                          OpenSSL::SSL
                          OpenSSL::Crypto
                          ZLIB::ZLIB)
    if (WIN32)
        set_property(TARGET CURL::CURL
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES
                              crypt32
                              wsock32
                              ws2_32)
    endif ()
    if (APPLE)
        set_property(TARGET CURL::CURL
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES ldap)
    endif ()
endif ()
