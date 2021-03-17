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

# Make our include guard clean against set -o nounset.
test -n "${CI_KOKORO_LIB_DOCKER_VARIABLES_SH__:-}" || declare -i CI_KOKORO_LIB_DOCKER_VARIABLES_SH__=0
if ((CI_KOKORO_LIB_DOCKER_VARIABLES_SH__++ != 0)); then
  return 0
fi # include guard

if [[ -f "${KOKORO_GFILE_DIR:-}/gcr-configuration.sh" ]]; then
  source "${KOKORO_GFILE_DIR:-}/gcr-configuration.sh"
fi

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
  DOCKER_IMAGE_BASENAME="ci-${DISTRO}"
  DOCKER_README_IMAGE_BASENAME="ci-readme-${DISTRO}"
  DOCKER_INSTALL_IMAGE_BASENAME="ci-install-${DISTRO}"
  if [[ -n "${DISTRO_VERSION:-}" ]]; then
    DOCKER_IMAGE_BASENAME="${DOCKER_IMAGE_BASENAME}-${DISTRO_VERSION}"
    DOCKER_README_IMAGE_BASENAME="${DOCKER_README_IMAGE_BASENAME}-${DISTRO_VERSION}"
    DOCKER_INSTALL_IMAGE_BASENAME="${DOCKER_INSTALL_IMAGE_BASENAME}-${DISTRO_VERSION}"
  fi
  readonly DOCKER_README_IMAGE_BASENAME
  readonly DOCKER_INSTALL_IMAGE_BASENAME
  readonly DOCKER_IMAGE_BASENAME

  if [[ -n "${PROJECT_ID:-}" ]]; then
    DOCKER_IMAGE_PREFIX="gcr.io/${PROJECT_ID}/google-cloud-cpp"
  else
    # We want a prefix that works when running interactively, so it must be a
    # (syntactically) valid project id, this works.
    DOCKER_IMAGE_PREFIX="gcr.io/cloud-cpp-reserved/google-cloud-cpp"
  fi
  readonly DOCKER_IMAGE_PREFIX

  IMAGE="${DOCKER_IMAGE_PREFIX}/${DOCKER_IMAGE_BASENAME}"
  README_IMAGE="${DOCKER_IMAGE_PREFIX}/${DOCKER_README_IMAGE_BASENAME}"
  INSTALL_IMAGE="${DOCKER_IMAGE_PREFIX}/${DOCKER_INSTALL_IMAGE_BASENAME}"
  readonly IMAGE
  readonly README_IMAGE
  readonly README_INSTALL_IMAGE

  BUILD_OUTPUT="cmake-out/${DOCKER_IMAGE_BASENAME}"
  BUILD_HOME="cmake-out/home/${DOCKER_IMAGE_BASENAME}"
  if [[ -n "${BUILD_NAME:-}" ]]; then
    BUILD_OUTPUT="${BUILD_OUTPUT}-${BUILD_NAME}"
    BUILD_HOME="${BUILD_HOME}-${BUILD_NAME}"
  fi
  readonly BUILD_OUTPUT
  readonly BUILD_NAME
fi
