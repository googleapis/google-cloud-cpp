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

# The `FindCurl` CMake module does not handle static libraries correctly, and
# does not produce consistent names for the CURL targets. This file first tries
# to load the CURL config files, and only if not found fallsback to the FindCurl
# module. In the latter case, it adjusts the targets to work consistently across
# CMake versions and to link the right dependencies when compiled statically.

find_package(CURL CONFIG QUIET)
if (CURL_FOUND)
    message(STATUS "CURL found using via CONFIG module")
else ()
    # As searching for libcurl using CONFIG mode failed, try again using the
    # CMake config module. We will need to fix up a few things if the module is
    # found this way.
    find_package(CURL REQUIRED)
    # Before CMake 3.12 the module does not define a target, compare:
    # https://cmake.org/cmake/help/v3.12/module/FindCURL.html vs
    # https://cmake.org/cmake/help/v3.11/module/FindCURL.html
    #
    # Manually define the target if it does not exist so the rest of the code
    # does not have to deal with these details:
    if (NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl UNKNOWN IMPORTED)
        set_property(
            TARGET CURL::libcurl
            APPEND
            PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}")
        set_property(
            TARGET CURL::libcurl
            APPEND
            PROPERTY IMPORTED_LOCATION "${CURL_LIBRARY}")
    endif ()
    # If the library is static, we need to explicitly link its dependencies. The
    # CMake module does not do so. However, we should not do so for shared
    # libraries, because the version of OpenSSL (for example) found by
    # find_package() may be newer than the version linked against libcurl.
    if ("${CURL_LIBRARY}" MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
        find_package(OpenSSL REQUIRED)
        find_package(ZLIB REQUIRED)
        set_property(
            TARGET CURL::libcurl
            APPEND
            PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto
                     ZLIB::ZLIB)
        message(STATUS "CURL linkage will be static")
        # On WIN32 and APPLE there are even more libraries needed for static
        # linking.
        if (WIN32)
            set_property(
                TARGET CURL::libcurl
                APPEND
                PROPERTY INTERFACE_LINK_LIBRARIES crypt32 wsock32 ws2_32)
        endif ()
        if (APPLE)
            set_property(
                TARGET CURL::libcurl
                APPEND
                PROPERTY INTERFACE_LINK_LIBRARIES ldap)
        endif ()
    else ()
        message(STATUS "CURL linkage will be non-static")
    endif ()
endif (CURL_FOUND)
