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

# A helper file for IncludeNlohmannJson. We need to download the json.hpp file.
# With old versions of cmake there is no way to disable the untar step from all
# downloads, and we get failures.  This is a custom cmake script to download
# that file.
set(JSON_URL
    "https://github.com/nlohmann/json/releases/download/v3.4.0/json.hpp")
set(
    JSON_SHA256 63da6d1f22b2a7bb9e4ff7d6b255cf691a161ff49532dcc45d398a53e295835f
    )
message("JSON_URL = ${JSON_URL}")
message("JSON_SHA256 = ${JSON_SHA256}")
message("DEST = ${DEST}")
file(DOWNLOAD "${JSON_URL}" "${DEST}/json.hpp"
     EXPECTED_HASH SHA256=${JSON_SHA256})
