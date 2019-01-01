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

if [[ "${TRAVIS_OS_NAME}" != "linux" ]]; then
  echo "Not a Linux-based build, skipping Linux-specific build steps."
  exit 0
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"

# The default user for a Docker container has uid 0 (root). To avoid creating
# root-owned files in the build directory we tell docker to use the current
# user ID, if known.
docker_uid="${UID:-0}"
if [[ "${docker_uid}" == "0" ]]; then
  # If the UID is 0, then the HOME directory will be set to /root, and we
  # need to mount the ccache files is /root/.ccache.
  docker_home=/root
else
  # If the UID is not zero then HOME is "/", and we need to mount the ccache
  # files in /.ccache.
  docker_home=""
fi

# Use a volume to store the cache files. This exports the cache files from the
# Docker container, and then we can save them for future Travis builds.
test -d "${PWD}/build-output/cache" || mkdir -p "${PWD}/build-output/cache"
test -d "${PWD}/build-output/ccache" || mkdir -p "${PWD}/build-output/ccache"

sudo docker run \
     --cap-add SYS_PTRACE \
     -it \
     --env DISTRO="${DISTRO}" \
     --env DISTRO_VERSION="${DISTRO_VERSION}" \
     --env CXX="${CXX}" \
     --env CC="${CC}" \
     --env NCPU="${NCPU:-2}" \
     --env BUILD_TYPE="${BUILD_TYPE:-Release}" \
     --env BUILD_TESTING="${BUILD_TESTING:-}" \
     --env CHECK_ABI="${CHECK_ABI:-}" \
     --env CHECK_STYLE="${CHECK_STYLE:-}" \
     --env SCAN_BUILD="${SCAN_BUILD:-}" \
     --env GENERATE_DOCS="${GENERATE_DOCS:-}" \
     --env TEST_INSTALL="${TEST_INSTALL:-}" \
     --env CREATE_GRAPHVIZ="${CREATE_GRAPHVIZ:-}" \
     --env CMAKE_FLAGS="${CMAKE_FLAGS:-}" \
     --env CBT=/usr/local/google-cloud-sdk/bin/cbt \
     --env CBT_EMULATOR=/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator \
     --env TERM="${TERM:-dumb}" \
     --user "${docker_uid}" \
     --volume "${PWD}":/v \
     --volume "${PWD}/build-output/cache":${docker_home}/.cache \
     --volume "${PWD}/build-output/ccache":${docker_home}/.ccache \
     --workdir /v \
     "${IMAGE}:tip" \
     "/v/ci/travis/build-docker.sh"
