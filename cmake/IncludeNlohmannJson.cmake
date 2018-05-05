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

include(ExternalProject)
ExternalProject_Add(nlohmann_json_project
        URL https://github.com/nlohmann/json/releases/download/v3.1.2/json.hpp
        URL_HASH SHA256=fbdfec4b4cf63b3b565d09f87e6c3c183bdd45c5be1864d3fcb338f6f02c1733
        DOWNLOAD_NO_EXTRACT 1
        PREFIX "${CMAKE_BINARY_DIR}/external/nlohmann_json"
        CONFIGURE_COMMAND ""
        # This is not great, we abuse the `build` step to create the target directory.
        # Unfortunately there is no way to specify two commands in the install step.
        BUILD_COMMAND ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/include/nlohmann
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/../json.hpp <INSTALL_DIR>/include/nlohmann/json.hpp
        LOG_DOWNLOAD ON
        LOG_INSTALL ON)
add_library(nlohmann_json INTERFACE)
add_dependencies(nlohmann_json nlohmann_json_project)
target_include_directories(nlohmann_json INTERFACE
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/external/nlohmann_json/include>
        $<INSTALL_INTERFACE:include>)
