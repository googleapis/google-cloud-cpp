#!/usr/bin/env bash
#
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

set -eu

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  PROJECT_ROOT="$(
    cd "$(dirname "$0")/.."
    pwd
  )"
  readonly PROJECT_ROOT
fi
source "${PROJECT_ROOT}/ci/etc/install-config.sh"

KERNEL="$(uname -s | tr '[:upper:]' '[:lower:]')"
readonly KERNEL
MACHINE="$(uname -m)"
if [[ "${KERNEL}" = "darwin" ]]; then
  # Bazel currently doesn't offer a darwin + arm package, but the x86_64
  # package works.
  MACHINE="x86_64"
fi
readonly MACHINE
readonly PLATFORM="${KERNEL}-${MACHINE}"

readonly BAZEL_VERSION="${GOOGLE_CLOUD_CPP_BAZEL_VERSION}"
readonly GITHUB_DL="https://github.com/bazelbuild/bazel/releases/download"
readonly SCRIPT_NAME="bazel-${BAZEL_VERSION}-installer-${PLATFORM}.sh"
readonly SCRIPT_HASH="${SCRIPT_NAME}.sha256"
curl -L "${GITHUB_DL}/${BAZEL_VERSION}/${SCRIPT_NAME}" -o "${SCRIPT_NAME}"
curl -L "${GITHUB_DL}/${BAZEL_VERSION}/${SCRIPT_HASH}" -o "${SCRIPT_HASH}"

# We want to protect against accidents (i.e., we don't want to download and
# execute a 404 page), not malice, so downloading the checksum and the file
# from the same source is Okay.
sha256sum --check "${SCRIPT_HASH}"

chmod +x "${SCRIPT_NAME}"
if [[ "${USER:-}" = "root" ]] || [[ "${USER+x}" = "" ]]; then
  "./${SCRIPT_NAME}"
else
  "./${SCRIPT_NAME}" --user
fi
