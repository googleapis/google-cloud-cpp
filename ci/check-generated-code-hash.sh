#!/usr/bin/env bash
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/lib/init.sh"
source module lib/io.sh

source "${PROJECT_ROOT}/ci/etc/generator-googleapis-commit-hash.sh"

if [[ "${CHECK_GENERATED_CODE_HASH}" != "yes" ]]; then
  echo "Skipping the generated code hash check as it is disabled for this build."
  exit 0
fi

readonly BAZEL_BIN=${BAZEL_BIN:-/usr/local/bin/bazel}
readonly BAZEL_DEPS_GOOGLEAPIS_HASH=$("${BAZEL_BIN}" query 'kind(http_archive, //external:com_google_googleapis)' --output=build | sed -n 's/^.*strip_prefix = "googleapis-\(\S*\)"\,.*$/\1/p')

if [[ "${BAZEL_DEPS_GOOGLEAPIS_HASH}" != "${GOOGLEAPIS_HASH_LAST_USED}" ]]; then
  io::log_red "Current bazel dependency googleapis hash: ${BAZEL_DEPS_GOOGLEAPIS_HASH}"
  io::log_red "does not match hash last used to generate code: ${GOOGLEAPIS_HASH_LAST_USED}"
  io::log_yellow "Run 'ci/kokoro/docker/build.sh generate-libraries' to update."
  exit 1
fi

exit 0
