# ~~~
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# Give application developers a hook to configure the version and hash
# downloaded from GitHub.
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA
    ""
    CACHE STRING "Deprecated. Use GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL")
mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA)
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    ""
    CACHE STRING
          "Deprecated. Use GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL_HASH")
mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256)

set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA
    "0b58a18a17a5091682c5943bbf8fd46775002227")
set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    "cdbc8dcc84578420d5526482841676de609e6c773c24089c66dc4a4287b0874c")

set(DOXYGEN_ALIASES
    "googleapis_link{2}=\"[\\1](https://github.com/googleapis/googleapis/blob/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\2)\""
    "googleapis_reference_link{1}=\"https://github.com/googleapis/googleapis/blob/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\1\""
)
