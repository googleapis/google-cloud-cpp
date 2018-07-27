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

include(external/zlib)

if (NOT TARGET protobuf_project)
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
        protobuf_project
        DEPENDS zlib_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/protobuf"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL https://github.com/google/protobuf/archive/v3.5.2.tar.gz
        URL_HASH
            SHA256=4ffd420f39f226e96aebc3554f9c66a912f6cad6261f39f194f16af8a1f6dab2
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
                          $ENV{CMAKE_FLAGS}
                          ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                          -DCMAKE_BUILD_TYPE=Debug
                          -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                          -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
                          -Dprotobuf_BUILD_TESTS=OFF
                          -H<SOURCE_DIR>/cmake
                          -B<BINARY_DIR>/Debug
        COMMAND ${CMAKE_COMMAND} $ENV{CMAKE_FLAGS}
                ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                -DCMAKE_PREFIX_PATH=<INSTALL_DIR> -Dprotobuf_BUILD_TESTS=OFF
                -H<SOURCE_DIR>/cmake -B<BINARY_DIR>/Release
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      Debug
                      ${PARALLEL}
        COMMAND ${CMAKE_COMMAND} --build Release ${PARALLEL}
        INSTALL_COMMAND ${CMAKE_COMMAND}
                        --build
                        Debug
                        --target
                        install
        COMMAND ${CMAKE_COMMAND} --build Release --target install
        BUILD_BYPRODUCTS
            <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}
            <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/libprotobuf${CMAKE_SHARED_LIBRARY_SUFFIX}
            <INSTALL_DIR>/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)
endif ()
