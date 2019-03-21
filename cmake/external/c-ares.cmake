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

if (NOT TARGET c_ares_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_C_ARES_URL
        "https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz")
    set(GOOGLE_CLOUD_CPP_C_ARES_SHA256
        "62dd12f0557918f89ad6f5b759f0bf4727174ae9979499f5452c02be38d9d3e8")

    set_external_project_build_parallel_level(PARALLEL)

    create_external_project_library_byproduct_list(c_ares_byproducts
                                                   ALWAYS_SHARED "cares")

    include(ExternalProject)
    externalproject_add(c_ares_project
                        EXCLUDE_FROM_ALL ON
                        PREFIX "${CMAKE_BINARY_DIR}/external/c-ares"
                        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
                        URL ${GOOGLE_CLOUD_CPP_C_ARES_URL}
                        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_C_ARES_SHA256}
                        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                        BUILD_COMMAND ${CMAKE_COMMAND}
                                      --build
                                      <BINARY_DIR>
                                      ${PARALLEL}
                        BUILD_BYPRODUCTS ${c_ares_byproducts}
                        LOG_DOWNLOAD ON
                        LOG_CONFIGURE ON
                        LOG_BUILD ON
                        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies c_ares_project)
    endif ()

    add_library(c-ares::cares INTERFACE IMPORTED)
    set_library_properties_for_external_project(c-ares::cares cares
                                                ALWAYS_SHARED)
    add_dependencies(c-ares::cares c_ares_project)
endif ()
