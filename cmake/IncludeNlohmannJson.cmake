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
        URL https://github.com/nlohmann/json/archive/v3.1.2.tar.gz
        URL_HASH SHA256=e8fffa6cbdb3c15ecdff32eebf958b6c686bc188da8ad5c6489462d16f83ae54
        PREFIX "${CMAKE_BINARY_DIR}/external/nlohmann_json"
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -DJSON_BuildTests=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -H<SOURCE_DIR> -B.build
        BUILD_COMMAND ${CMAKE_COMMAND} --build .build --target all
        INSTALL_COMMAND ${CMAKE_COMMAND} --build .build --target install
        USES_TERMINAL_DOWNLOAD 1
        LOG_DOWNLOAD ON
        LOG_INSTALL ON)
add_library(nlohmann_json INTERFACE)
add_dependencies(nlohmann_json nlohmann_json_project)
target_include_directories(nlohmann_json INTERFACE
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/external/nlohmann_json/include>
        $<INSTALL_INTERFACE:include>)
