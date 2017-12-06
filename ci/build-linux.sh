#!/bin/sh
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

if [ "${TRAVIS_OS_NAME}" != "linux" ]; then
  echo "Not a Linux-based build, skipping Linux-specific build steps."
  exit 0
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
echo "IMAGE = ${IMAGE}"

sudo docker build -t "${IMAGE}:tip" \
     --build-arg DISTRO_VERSION="${DISTRO_VERSION}" \
     --build-arg CXX="${CXX}" \
     --build-arg CC="${CC}" \
     --build-arg NCPU="${NCPU:-2}" \
     --build-arg BUILD_TYPE="${BUILD_TYPE:-Release}" \
     --build-arg CHECK_STYLE="${CHECK_STYLE:-}" \
     --build-arg GENERATE_DOCS="${GENERATE_DOCS:-}" \
     --build-arg SANITIZE_ADDRESS="${SANITIZE_ADDRESS:-}" \
     --build-arg SANITIZE_LEAKS="${SANITIZE_LEAKS:-}" \
     --build-arg SANITIZE_MEMORY="${SANITIZE_MEMORY:-}" \
     --build-arg SANITIZE_THREAD="${SANITIZE_THREAD:-}" \
     --build-arg SANITIZE_UNDEFINED="${SANITIZE_UNDEFINED:-}" \
     -f "ci/Dockerfile.${DISTRO}" .
