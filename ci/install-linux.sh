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
if [ "${TRAVIS_OS_NAME}" != "linux" ]; then
  echo "Not a Linux-based build; skipping Docker image creation."
  exit 0
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
# Make three attempts to build the Docker image.  From time to time, the image
# creation fails because some apt server times out, and more often than not,
# restarting the build is successful.

# Initially, wait at least 3 minutes (the times are in seconds), because it
# makes no sense to try faster.  We currently have 20+ minutes of remaining
# budget to complete a build, and we should be as nice as possible to the
# servers that provide the packages.
min_wait=180
for i in 1 2 3; do
  sudo docker build -t "${IMAGE}:tip" \
       --build-arg DISTRO_VERSION="${DISTRO_VERSION}" \
       -f "ci/Dockerfile.${DISTRO}" ci
  if [ $? -eq 0 ]; then
    exit 0
  fi
  # Sleep for a few minutes before trying again.
  readonly PERIOD=$[ (${RANDOM} % 60) + min_wait ]
  echo "Fetch failed; trying again in ${PERIOD} seconds."
  sleep ${PERIOD}s
  min_wait=$[ min_wait * 2 ]
done
