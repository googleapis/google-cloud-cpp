#!/usr/bin/env bash
# Copyright 2022 Google LLC
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

set -euo pipefail

# Determine the googleapis HEAD commit and the checksum of its tarball.
REPO="googleapis/googleapis"
BRANCH="master"
COMMIT=$(curl -sSL -H "Accept: application/vnd.github.VERSION.sha" \
  "https://api.github.com/repos/${REPO}/commits/${BRANCH}")
DOWNLOAD="$(mktemp)"
curl -sSL "https://github.com/${REPO}/archive/${COMMIT}.tar.gz" -o "${DOWNLOAD}"
gsutil -q cp "${DOWNLOAD}" "gs://cloud-cpp-community-archive/com_google_googleapis/${COMMIT}.tar.gz"
SHA256=$(sha256sum "${DOWNLOAD}" | sed "s/ .*//")
rm -f "${DOWNLOAD}"

# Update the Bazel dependency.
sed -i -f - bazel/google_cloud_cpp_deps.bzl <<EOT
  /name = "com_google_googleapis",/,/sha256 = "/ {
    s;/${REPO}/archive/.*.tar.gz",;/${REPO}/archive/${COMMIT}.tar.gz",;
    s;/com_google_googleapis/.*.tar.gz",;/com_google_googleapis/${COMMIT}.tar.gz",;
    s/strip_prefix = "googleapis-.*",/strip_prefix = "googleapis-${COMMIT}",/
    s/sha256 = ".*",/sha256 = "${SHA256}",/
  }
EOT

# Update the CMake dependency.
sed -i -f - cmake/GoogleapisConfig.cmake <<EOT
  /^set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA$/ {
    n
    s/".*"/"${COMMIT}"/
  }
  /^set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256$/ {
    n
    s/".*"/"${SHA256}"/
  }
EOT
