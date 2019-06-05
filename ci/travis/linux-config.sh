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

if [[ -n "${IMAGE+x}" ]]; then
  echo "IMAGE is already defined."
else
  readonly IMAGE="gcpp-ci-${DISTRO}-${DISTRO_VERSION}"
  suffix=""
  if [[ -n "${CC:-}" ]]; then
    suffix="${suffix}-${CC}"
  fi
  suffix="${suffix}-${BUILD_TYPE:-Release}"
  if [[ "${SCAN_BUILD+x}" = "yes" ]]; then
    suffix="${suffix}-scan"
  fi
  if [[ -z "${CMAKE_FLAGS:-}" ]]; then
      /bin/true
  elif echo "${CMAKE_FLAGS}" | grep -Eq BUILD_SHARED_LIBS=yes; then
      suffix="${suffix}-shared"
  elif echo "${CMAKE_FLAGS}" | grep -Eq GOOGLE_CLOUD_CPP_CLANG_TIDY=yes; then
      suffix="${suffix}-tidy"
  elif echo "${CMAKE_FLAGS}" | grep -Eq TEST_INSTALL=yes; then
      suffix="${suffix}-install"
  elif echo "${CMAKE_FLAGS}" | grep -Eq GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=no; then
      suffix="${suffix}-noex"
  fi
  readonly BUILD_OUTPUT="cmake-out/${IMAGE}${suffix}"
  readonly DOCKER_CCACHE_DIR="cmake-out/ccache/${IMAGE}${suffix}"
fi
