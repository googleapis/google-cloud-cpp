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
        LIST_SEPARATOR |
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                   # libcurl automatically enables a number of protocols. With
                   # static libraries this is a problem. The indirect
                   # dependencies, such as libldap, become hard to predict and
                   # manage. Setting HTTP_ONLY=ON disables all optional
                   # protocols and meets our needs. If the application needs
                   # a version of libcurl with other protocols enabled they
                   # can select it using GOOGLE_CLOUD_CPP_CURL_PROVIDER=package.
                   -DHTTP_ONLY=ON
                   -DCMAKE_ENABLE_OPENSSL=ON
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                   -DENABLE_ARES=ON
                   -DCURL_STATICLIB=$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>
                   -DCMAKE_DEBUG_POSTFIX=
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${curl_byproducts}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies curl_project)
    endif ()

    include(ExternalProjectHelper)
    add_library(CURL::libcurl INTERFACE IMPORTED)
    add_dependencies(CURL::libcurl curl_project)
    set_library_properties_for_external_project(CURL::libcurl curl)
    set_property(TARGET CURL::libcurl
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          c-ares::cares
                          OpenSSL::SSL
                          OpenSSL::Crypto
                          ZLIB::ZLIB)
    if (WIN32)
        set_property(TARGET CURL::libcurl
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES
                              crypt32
                              wsock32
                              ws2_32)
    endif ()
    if (APPLE)
        set_property(TARGET CURL::libcurl
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES ldap)
    endif ()
endif ()
