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

include(ExternalProject)
externalproject_add(
    nlohmann_json_project
    PREFIX "${CMAKE_BINARY_DIR}/external/nlohmann_json"
    DOWNLOAD_COMMAND ${CMAKE_COMMAND}
                     -DDEST=<INSTALL_DIR>/src
                     -P
                     ${PROJECT_SOURCE_DIR}/cmake/DownloadNlohmannJson.cmake
    CONFIGURE_COMMAND "" # This is not great, we abuse the `build` step to
                         # create the target directory. Unfortunately there is
                         # no way to specify two commands in the install step.
    BUILD_COMMAND ${CMAKE_COMMAND}
                  -E
                  make_directory
                  <INSTALL_DIR>/include
    INSTALL_COMMAND
        ${CMAKE_COMMAND}
        -E
        copy
        <INSTALL_DIR>/src/json.hpp
        <INSTALL_DIR>/include/google/cloud/storage/internal/nlohmann_json.hpp
    LOG_DOWNLOAD ON
    LOG_INSTALL ON)

add_library(nlohmann_json INTERFACE)
add_dependencies(nlohmann_json nlohmann_json_project)
target_include_directories(
    nlohmann_json
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/external/nlohmann_json/include>
        $<INSTALL_INTERFACE:include>)
