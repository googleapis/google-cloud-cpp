# ~~~
# Copyright 2018 Google Inc.
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

# gRPC always requires thread support.
find_package(Threads REQUIRED)

# Configure the gRPC dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
if (APPLE)
    # Still cannot make libcurl work as an external project on macOS. The
    # default static build just does not work for me.
    set(GOOGLE_CLOUD_CPP_CURL_PROVIDER "package"
        CACHE STRING "How to find the libcurl library.")
else()
    set(GOOGLE_CLOUD_CPP_CURL_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
        CACHE STRING "How to find the libcurl.")
endif ()
set_property(CACHE GOOGLE_CLOUD_CPP_CURL_PROVIDER
             PROPERTY STRINGS "external" "package")

if ("${GOOGLE_CLOUD_CPP_CURL_PROVIDER}" STREQUAL "external")
    include(external/curl)
elseif("${GOOGLE_CLOUD_CPP_CURL_PROVIDER}" STREQUAL "package")
    # Search for libcurl, in CMake 3.5 this does not define a target, but it
    # will in 3.12 (see https://cmake.org/cmake/help/git-
    # stage/module/FindCURL.html for details).  Until then, define the target
    # ourselves if it is missing.
    find_package(CURL REQUIRED)
    if (NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl UNKNOWN IMPORTED)
        set_property(TARGET CURL::libcurl
                     APPEND
                     PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                              "${CURL_INCLUDE_DIR}")
        set_property(TARGET CURL::libcurl
                     APPEND
                     PROPERTY IMPORTED_LOCATION "${CURL_LIBRARY}")
    endif ()
    # If the library is static, we need to explicitly link its dependencies.
    # However, we should not do so for shared libraries, because the version of
    # OpenSSL (for example) found by find_package() may be newer than the
    # version linked against libcurl.
    if ("${CURL_LIBRARY}" MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
        find_package(OpenSSL REQUIRED)
        find_package(ZLIB REQUIRED)
        set_property(TARGET CURL::libcurl
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES
                              OpenSSL::SSL
                              OpenSSL::Crypto
                              ZLIB::ZLIB)
        message(STATUS "CURL linkage will be static")
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
    else()
        message(STATUS "CURL linkage will be non-static")
    endif ()
endif ()
