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

if (NOT TARGET crc32c_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_CRC32C_URL
        "https://github.com/google/crc32c/archive/1.0.5.tar.gz")
    set(GOOGLE_CLOUD_CPP_CRC32C_SHA256
        "c2c0dcc8d155a6a56cc8d56bc1413e076aa32c35784f4d457831e8ccebd9260b")

    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles"
        OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

    create_external_project_library_byproduct_list(crc32c_byproducts "crc32c")

    include(ExternalProject)
    externalproject_add(crc32c_project
                        EXCLUDE_FROM_ALL ON
                        PREFIX "${CMAKE_BINARY_DIR}/external/crc32c"
                        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
                        URL ${GOOGLE_CLOUD_CPP_CRC32C_URL}
                        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_CRC32C_SHA256}
                        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                                   -DCMAKE_BUILD_TYPE=Release
                                   -DCRC32C_BUILD_TESTS=OFF
                                   -DCRC32C_BUILD_BENCHMARKS=OFF
                                   -DCRC32C_USE_GLOG=OFF
                                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                                   -DCMAKE_PREFIX_PATH=<INSTALL_DIR>
                        BUILD_COMMAND ${CMAKE_COMMAND}
                                      --build
                                      <BINARY_DIR>
                                      ${PARALLEL}
                        BUILD_BYPRODUCTS ${crc32c_byproducts}
                        LOG_DOWNLOAD ON
                        LOG_CONFIGURE ON
                        LOG_BUILD ON
                        LOG_INSTALL ON)

    include(ExternalProjectHelper)
    add_library(Crc32c::crc32c INTERFACE IMPORTED)
    add_dependencies(Crc32c::crc32c crc32c_project)
    set_library_properties_for_external_project(Crc32c::crc32c crc32c)
endif ()
