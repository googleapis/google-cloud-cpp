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

include(ExternalProjectHelper)

if (NOT TARGET nlohmann-json-project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_NLOHMANN_JSON_URL
        "https://github.com/nlohmann/json/releases/download/v3.4.0/include.zip")
    set(GOOGLE_CLOUD_CPP_NLOHMANN_JSON_SHA256
        bfec46fc0cee01c509cf064d2254517e7fa80d1e7647fea37cf81d97c5682bdc)

    set_external_project_build_parallel_level(PARALLEL)
    set_external_project_vars()

    include(ExternalProject)
    ExternalProject_Add(
        nlohmann-json-project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/nlohmann_json"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_NLOHMANN_JSON_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_NLOHMANN_JSON_SHA256}
        LIST_SEPARATOR |
        CONFIGURE_COMMAND
            ""
            # ~~~
            # This is not great, we abuse the `build` step to create the target
            # directory. Unfortunately there is no way to specify two commands in
            # the install step.
            # ~~~
        BUILD_COMMAND ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/include
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/nlohmann
            <INSTALL_DIR>/include/nlohmann
            # TODO(#2874) - switch these to ON
        LOG_DOWNLOAD OFF
        LOG_CONFIGURE OFF
        LOG_BUILD OFF
        LOG_INSTALL OFF)
endif ()
