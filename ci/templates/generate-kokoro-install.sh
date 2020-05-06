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

if [[ "${BASH_VERSINFO[0]}" -lt 4 ]] ||
  [[ "${BASH_VERSINFO[0]}" -eq 4 && "${BASH_VERSINFO[1]}" -lt 4 ]]; then
  echo "This script requires BASH >= 4.4, found ${BASH_VERSION}"
  exit 1
fi

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  PROJECT_ROOT="$(
    cd "$(dirname "$0")/../.."
    pwd
  )"
  readonly PROJECT_ROOT
fi

DESTINATION_ROOT="$1"
readonly DESTINATION_ROOT

source "${DESTINATION_ROOT}/ci/etc/repo-config.sh"

BUILD_NAMES=(
  centos-7
  centos-8
  debian-buster
  debian-stretch
  fedora
  opensuse-leap
  opensuse-tumbleweed
  ubuntu-xenial
  ubuntu-bionic
  ubuntu-focal
)
readonly BUILD_NAMES

# shellcheck disable=SC1091
source "${PROJECT_ROOT}/ci/templates/kokoro/docker-fragments-functions.sh"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/ci/templates/kokoro/docker-fragments.sh"
# shellcheck disable=SC1091
source "${DESTINATION_ROOT}/ci/etc/kokoro/install/project-config.sh"

generate_dockerfile() {
  local -r build="$1"
  local -r path="kokoro/install/Dockerfile.${build}"
  local -r target="${DESTINATION_ROOT}/ci/${path}"
  local -r source="${PROJECT_ROOT}/ci/templates/${path}.in"

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
    "BUILD_AND_TEST_PROJECT_FRAGMENT" \
    <"${source}" |
    sed -e "s/Copyright [0-9][0-9][0-9][0-9]/Copyright ${ORIGINAL_COPYRIGHT_YEAR[${build}]}/" \
      >"${target}"
}

# Remove all files except common.cfg; any other files we want to preserve will be created again.
git -C "${DESTINATION_ROOT}" rm -fr --ignore-unmatch "ci/kokoro/install"
git -C "${DESTINATION_ROOT}" reset HEAD "ci/kokoro/install/common.cfg"
git -C "${DESTINATION_ROOT}" checkout -- "ci/kokoro/install/common.cfg"

replace_fragments \
  "WARNING_GENERATED_FILE_FRAGMENT" \
  "GOOGLE_CLOUD_CPP_REPOSITORY" \
  <"${PROJECT_ROOT}/ci/templates/kokoro/install/build.sh.in" \
  >"${DESTINATION_ROOT}/ci/kokoro/install/build.sh"
chmod 755 "${DESTINATION_ROOT}/ci/kokoro/install/build.sh"

for build in "${BUILD_NAMES[@]}"; do
  # We need these empty files because Kokoro does not work unless they exist.
  touch "${DESTINATION_ROOT}/ci/kokoro/install/${build}.cfg"
  touch "${DESTINATION_ROOT}/ci/kokoro/install/${build}-presubmit.cfg"
  generate_dockerfile "${build}"
done

# The project-config.sh file may specify a number of "frozen" files, that is,
# files that are not modified by this script.
for file in "${FROZEN_FILES[@]}"; do
  echo "Restoring ... ${file}"
  git -C "${DESTINATION_ROOT}" reset HEAD "${file}"
  git -C "${DESTINATION_ROOT}" checkout -- "${file}"
done

git -C "${DESTINATION_ROOT}" add "ci/kokoro/install"
