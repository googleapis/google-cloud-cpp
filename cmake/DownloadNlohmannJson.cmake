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

# A helper file for IncludeNlohmannJson.
# We need to download the json.hpp file.  With old versions of cmake there
# is no way to disable the untar step from all downloads, and we get
# failures.  This is a custom cmake script to download that file.
set(JSON_URL "https://github.com/nlohmann/json/releases/download/v3.1.2/json.hpp")
#set(JSON_URL "https://github-production-release-asset-2e65be.s3.amazonaws.com/11171548/fde59cb6-27cb-11e8-98f3-11de19cf3b61?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAIWNJYAX4CSVEH53A%2F20180622%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20180622T053048Z&X-Amz-Expires=300&X-Amz-Signature=2c620b270cc09a875dc77a449ed3c03efc92370af665bd1a4ebeb27b39bb8da0&X-Amz-SignedHeaders=host&actor_id=0&response-content-disposition=attachment%3B%20filename%3Djson.hpp&response-content-type=application%2Foctet-stream")
set(JSON_SHA256 fbdfec4b4cf63b3b565d09f87e6c3c183bdd45c5be1864d3fcb338f6f02c1733)
message("JSON_URL = ${JSON_URL}")
message("JSON_SHA256 = ${JSON_SHA256}")
message("DEST = ${DEST}")
file(DOWNLOAD "${JSON_URL}" "${DEST}/json.hpp"
        EXPECTED_HASH SHA256=${JSON_SHA256})
