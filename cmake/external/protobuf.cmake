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

include(ExternalProjectHelper)
find_package(Threads REQUIRED)
include(external/zlib)

if (NOT TARGET protobuf_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_PROTOBUF_URL
        "https://github.com/google/protobuf/archive/v3.6.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_PROTOBUF_SHA256
        "3d4e589d81b2006ca603c1ab712c9715a76227293032d05b26fca603f90b3f5b")

    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles"
        OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

    create_external_project_library_byproduct_list(protobuf_byproducts
                                                   "protobuf")

    include(ExternalProject)
    externalproject_add(
        protobuf_project
        DEPENDS zlib_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/protobuf"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_PROTOBUF_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_PROTOBUF_SHA256}
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
                          -G${CMAKE_GENERATOR}
                          ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                          -DCMAKE_BUILD_TYPE=Debug
                          -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                          -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                          -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
                          -Dprotobuf_BUILD_TESTS=OFF
                          -Dprotobuf_DEBUG_POSTFIX=
                          -H<SOURCE_DIR>/cmake
                          -B<BINARY_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${protobuf_byproducts}
                         <INSTALL_DIR>/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    add_library(protobuf::libprotobuf INTERFACE IMPORTED)
    add_dependencies(protobuf::libprotobuf protobuf_project)
    set_library_properties_for_external_project(protobuf::libprotobuf protobuf)
    set_property(TARGET protobuf::libprotobuf
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          protobuf::libprotobuf
                          ZLIB::ZLIB
                          Threads::Threads)
endif ()
