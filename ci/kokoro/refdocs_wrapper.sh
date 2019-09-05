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

if [[ -z "${BASE_IMAGE:-}" ]]; then
  BASE_IMAGE="gcr.io/cloud-devrel-kokoro-resources/cpp/refdocs-base"
fi

# work from the git root directory
cd "$(dirname "$0")/../../"

# Download the base image
has_cache="false"
gcloud auth configure-docker
if docker pull "${BASE_IMAGE}"; then
  echo "Base image successfully downloaded."
  has_cache="true"
fi

base_image_build_flags=(
  "-t" "${BASE_IMAGE}"
  "-f" "ci/kokoro/docker/Dockerfile.ubuntu-install"
  "--build-arg" "NCPU=$(nproc)"
)

if "${has_cache}"; then
  base_image_build_flags+=(
    "--cache-from=${BASE_IMAGE}"
  )
fi

update_cache="false"
if docker build "${base_image_build_flags[@]}" ci; then
  update_cache="true"
  echo "Base image created $(date)."
  docker image ls | grep "${BASE_IMAGE}"
else
  "Failed creating base image $(date)."
  if "${has_cache}"; then
    echo "Continue the build with the cache."
  else
    exit 1
  fi
fi

if "${update_cache}"; then
  docker push "${BASE_IMAGE}"
fi

docker build -t "${TRAMPOLINE_IMAGE}" -f ci/kokoro/Dockerfile.refdocs ci
docker push "${TRAMPOLINE_IMAGE}"

exec ci/kokoro/trampoline.sh
