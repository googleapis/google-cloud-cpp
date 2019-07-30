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

if [[ -z "${NCPU+x}" ]]; then
  # Mac doesn't have nproc. Run the equivalent.
  if [[ "$OSTYPE" == "darwin"* ]]; then
    NCPU=$(sysctl -n hw.physicalcpu)
  else
    NCPU=$(nproc)
  fi
  export NCPU
fi

if [[ -n "${IMAGE+x}" ]]; then
  echo "IMAGE is already defined."
else
  if [[ -n "${DISTRO_VERSION+x}" ]]; then
    readonly IMAGE="gcpp-ci-${DISTRO}-${DISTRO_VERSION}"
    readonly BUILD_OUTPUT="cmake-out/${IMAGE}-${BUILD_NAME}"
    readonly BUILD_HOME="cmake-out/home/${IMAGE}-${BUILD_NAME}"
  fi
fi
