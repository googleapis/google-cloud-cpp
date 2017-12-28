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
  echo "Not a Linux-based build; skipping Docker installation and restore from cache."
  exit 0
fi

if [ "${CI:-}" = "true" ]; then
  # Running in the CI environment, upgrade Docker.
  sudo apt-get update
  sudo apt-get install -y docker-ce
  sudo docker --version
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
sudo docker build -t "${IMAGE}:tip" \
     --build-arg DISTRO_VERSION="${DISTRO_VERSION}" \
     -f "ci/Dockerfile.${DISTRO}" ci
