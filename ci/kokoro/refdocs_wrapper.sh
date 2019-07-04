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

set -euo pipefail

if [[ -z "${TRAMPOLINE_IMAGE:-}" ]]; then
  TRAMPOLINE_IMAGE="gcr.io/cloud-devrel-kokoro-resources/cpp/refdocs"
fi

# work from the git root directory
cd "$(dirname "$0")/../../"

if [[ "${BUILD_DOCKER_IMAGE:-false}" == "true" ]]; then
  # First build the docker image with dependency installed
  docker build -t gcr.io/cloud-devrel-kokoro-resources/cpp/refdoc-base \
    --build-arg NCPU="$(nproc)" \
    -f ci/kokoro/docker/Dockerfile.ubuntu-install ci

  docker build -t "${TRAMPOLINE_IMAGE}" -f ci/kokoro/Dockerfile.refdocs ci

  gcloud auth configure-docker
  docker push "${TRAMPOLINE_IMAGE}"
fi

exec ci/kokoro/trampoline.sh
