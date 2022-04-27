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
    "a0d4c5c2a714a7317714ba6f0066a74ec8a9c10b"
    CACHE STRING "Configure the commit SHA (or tag) for the googleapis protos.")
mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA)
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    "523e5986bdb7f001807a96373f1f6a05833cebb98314bc34d0defd9126772285"
    CACHE STRING "Configure the SHA256 checksum of the googleapis tarball.")
mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256)

set(DOXYGEN_ALIASES
    "googleapis_link{2}=\"[\\1](https://github.com/googleapis/googleapis/blob/${GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\2)\""
    "googleapis_reference_link{1}=\"https://github.com/googleapis/googleapis/blob/${GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}/\\1\""
)
