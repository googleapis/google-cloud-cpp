#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

# Create a Docker image with all the dependencies necessary to build the
# project.
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/kokoro/docker/define-docker-variables.sh"

cd "${PROJECT_ROOT}"
sudo docker build -t "${IMAGE}:tip" \
     --build-arg NCPU="${NCPU:-4}" \
     --build-arg DISTRO_VERSION="${DISTRO_VERSION}" \
     -f "ci/kokoro/docker/Dockerfile.${DISTRO}" ci
