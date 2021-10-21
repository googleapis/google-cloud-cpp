#!/bin/bash
#
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

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_VCPKG_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

TIMEFORMAT="==> 🕑 vcpkg installed in %R seconds"
time {
  VCPKG_RELEASE_VERSION="6e024e744e7717c06ddacd5089401109c6298553"
  VCPKG_ROOT_DIR="${HOME}/vcpkg-${VCPKG_RELEASE_VERSION}"
  io::log_h2 "Installing vcpkg ${VCPKG_RELEASE_VERSION} -> ${VCPKG_ROOT_DIR}"
  if [[ ! -d "${VCPKG_ROOT_DIR}" ]]; then
    mkdir -p "${VCPKG_ROOT_DIR}"
    # vcpkg needs git history to support versioning, so we clone a recent
    # release tag rather than just extracting a tarball without history.
    git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT_DIR}"
    git -C "${VCPKG_ROOT_DIR}" checkout "${VCPKG_RELEASE_VERSION}"
    pwd
  fi
  env -C "${VCPKG_ROOT_DIR}" CC="ccache ${CC}" CXX="ccache ${CXX}" \
    ./bootstrap-vcpkg.sh
}

# Outputs the root directory where vcpkg is installed (and bootstrapped)
function vcpkg::root_dir() {
  echo "${VCPKG_ROOT_DIR}"
}
