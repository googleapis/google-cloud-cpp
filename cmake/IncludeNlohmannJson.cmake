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

function (find_nlohmann_json)
    find_package(nlohmann_json CONFIG QUIET)
    if (nlohmann_json_FOUND)
        return()
    endif ()
    # As a fall back, try finding the header. Since this is a header-only
    # library that is all we need.
    find_path(GOOGLE_CLOUD_CPP_NLOHMANN_JSON_HEADER "nlohmann/json.hpp"
              REQUIRED)
    add_library(nlohmann_json::nlohmann_json UNKNOWN IMPORTED)
    set_property(
        TARGET nlohmann_json::nlohmann_json
        APPEND
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 ${GOOGLE_CLOUD_CPP_NLOHMANN_JSON_HEADER})
endfunction ()

find_nlohmann_json()
