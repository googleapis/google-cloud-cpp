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

readonly PLATFORM=$(printf "%s-%s" "$(uname -s)" "$(uname -m)" \
  |  tr '[:upper:]' '[:lower:]')

readonly BAZEL_VERSION="0.24.0"
readonly GITHUB_DL="https://github.com/bazelbuild/bazel/releases/download"
readonly SCRIPT_NAME="bazel-${BAZEL_VERSION}-installer-${PLATFORM}.sh"
wget -q "${GITHUB_DL}/${BAZEL_VERSION}/${SCRIPT_NAME}"
wget -q "${GITHUB_DL}/${BAZEL_VERSION}/${SCRIPT_NAME}.sha256"

# We want to protect against accidents (i.e., we don't want to download and
# execute a 404 page), not malice, so downloading the checksum and the file
# from the same source is Okay.
sha256sum --check "${SCRIPT_NAME}.sha256"

chmod +x "${SCRIPT_NAME}"
if [[ "${USER:-}" = "root" ]] || [[ "${USER+x}" = "" ]]; then
  "./${SCRIPT_NAME}"
else
  "./${SCRIPT_NAME}" --user
fi
