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
    "ebf47e25ff363de57a6036562504db6caf3d8b89")
set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    "f21b0dbb4a18b2b4af77601049dc36ecb7dd1f4b05a2e38a05f456ffd1e258cc")

set(DOXYGEN_ALIASES
    "googleapis_link{2}=\"[\\1](https://github.com/googleapis/googleapis/blob/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\2)\""
    "googleapis_reference_link{1}=\"https://github.com/googleapis/googleapis/blob/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\1\""
)
