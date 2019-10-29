#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

if [[ $# -ne 1 ]]; then
  echo "Usage: $(basename "$0") <destination-ci-directory>"
  exit 1
fi

DESTINATION_ROOT="$1"
readonly DESTINATION_ROOT

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  PROJECT_ROOT="$(cd "$(dirname "$0")/../.."; pwd)"
  readonly PROJECT_ROOT
fi

source "${DESTINATION_ROOT}/ci/etc/repo-config.sh"

BUILD_NAMES=(
  centos-7
  centos-8
  debian-buster
  debian-stretch
  fedora
  opensuse-leap
  opensuse-tumbleweed
  ubuntu-trusty
  ubuntu-xenial
  ubuntu-bionic
)
readonly BUILD_NAMES

# shellcheck source=../../ci/etc/kokoro/docker-fragments.sh
source "${PROJECT_ROOT}/ci/templates/kokoro/docker-fragments-functions.sh"
# shellcheck source=../../ci/etc/kokoro/docker-fragments.sh
source "${PROJECT_ROOT}/ci/templates/kokoro/docker-fragments.sh"

generate_dockerfile() {
  local -r build="$1"

  target="${DESTINATION_ROOT}/ci/kokoro/readme/Dockerfile.${build}"
  echo "Generating ${target}"
  replace_fragments \
        "WARNING_GENERATED_FILE_FRAGMENT" \
        "INSTALL_PROTOBUF_FROM_SOURCE" \
        "INSTALL_C_ARES_FROM_SOURCE" \
        "INSTALL_GRPC_FROM_SOURCE" \
        "INSTALL_CPP_CMAKEFILES_FROM_SOURCE" \
        "INSTALL_GOOGLETEST_FROM_SOURCE" \
        "INSTALL_CRC32C_FROM_SOURCE" \
        "INSTALL_GOOGLE_CLOUD_CPP_COMMON_FROM_SOURCE" \
        "BUILD_PROJECT_CMAKE_SUPER_FRAGMENT" \
    <"${PROJECT_ROOT}/ci/templates/kokoro/readme/Dockerfile.${build}.in" \
    >"${target}"
}

# Remove all files, any files we want to preserve will be created again.
git -C "${DESTINATION_ROOT}" rm -fr --ignore-unmatch "ci/kokoro/readme"
mkdir -p "${DESTINATION_ROOT}/ci/kokoro/readme"

replace_fragments \
    "WARNING_GENERATED_FILE_FRAGMENT" \
    <"${PROJECT_ROOT}/ci/templates/kokoro/readme/build.sh.in" \
    >"${DESTINATION_ROOT}/ci/kokoro/readme/build.sh"
chmod 755 "${DESTINATION_ROOT}/ci/kokoro/readme/build.sh"
git -C "${DESTINATION_ROOT}" add "ci/kokoro/readme/build.sh"

replace_fragments \
    "WARNING_GENERATED_FILE_FRAGMENT" \
    < "${PROJECT_ROOT}/ci/templates/kokoro/readme/common.cfg.in" \
    >"${DESTINATION_ROOT}/ci/kokoro/readme/common.cfg"
git -C "${DESTINATION_ROOT}" add "ci/kokoro/readme/common.cfg"

for build in "${BUILD_NAMES[@]}"; do
  # We need these empty files because Kokoro does not work unless they exist.
  touch "${DESTINATION_ROOT}/ci/kokoro/readme/${build}.cfg"
  touch "${DESTINATION_ROOT}/ci/kokoro/readme/${build}-presubmit.cfg"
  generate_dockerfile "${build}"
done

git -C "${DESTINATION_ROOT}" add "ci/kokoro/readme"
