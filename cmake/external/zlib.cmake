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

if (NOT TARGET zlib_project)
    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles"
        OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

    include(ExternalProject)
    externalproject_add(
        zlib_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/zlib"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL https://github.com/madler/zlib/archive/v1.2.11.tar.gz
        URL_HASH
            SHA256=629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
                          $ENV{CMAKE_FLAGS}
                          ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                          -DCMAKE_BUILD_TYPE=Release
                          -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                          -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
                          -H<SOURCE_DIR>
                          -B<BINARY_DIR>/Release
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      Release
                      ${PARALLEL}
        INSTALL_COMMAND ${CMAKE_COMMAND}
                        --build
                        Release
                        --target
                        install
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}
                         <INSTALL_DIR>/lib/libz${CMAKE_STATIC_LIBRARY_SUFFIX}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
