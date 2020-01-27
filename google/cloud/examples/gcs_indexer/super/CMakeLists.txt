# ~~~
# Copyright 2020 Google LLC
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

cmake_minimum_required(VERSION 3.10)
project(cloud-run-cpp-build CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CLOUD_RUN_CPP_ROOT "${PROJECT_SOURCE_DIR}/..")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../super")

include(ExternalProjectHelper)
include(external/google-cloud-cpp-common)
include(external/google-cloud-cpp)
include(external/google-cloud-cpp-spanner)
include(external/boost)

set(GOOGLE_CLOUD_CPP_DEPENDENCIES_LIST
    google-cloud-cpp-spanner-project
        google-cloud-cpp-project
        google-cloud-cpp-common-project
        boost-project)

set_external_project_vars()
externalproject_add(
    google-run-cpp-project
    DEPENDS ${GOOGLE_CLOUD_CPP_DEPENDENCIES_LIST}
    EXCLUDE_FROM_ALL OFF
    PREFIX "${CMAKE_BINARY_DIR}"
    INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
    SOURCE_DIR
    "${CLOUD_RUN_CPP_ROOT}"
    LIST_SEPARATOR
    |
    BUILD_ALWAYS ON
    CMAKE_ARGS -G${CMAKE_GENERATOR}
               -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
               -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
               -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND ${CMAKE_COMMAND}
                  --build
                  <BINARY_DIR>
                  ${PARALLEL}
    TEST_COMMAND ""
    LOG_DOWNLOAD OFF
    LOG_CONFIGURE OFF
    LOG_BUILD OFF
    LOG_INSTALL OFF)

# This makes it easy to compile (or install) the dependencies before the
# project.
add_custom_target(project-dependencies)
add_dependencies(project-dependencies ${GOOGLE_CLOUD_CPP_DEPENDENCIES_LIST})
