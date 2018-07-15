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

if [ "${TRAVIS_OS_NAME}" != "linux" ]; then
  echo "Not a Linux-based build, skipping Linux-specific build steps."
  exit 0
fi

readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
if [ "${IMAGE}" = "cached-centos-7" ]; then
  build_script="scl enable devtoolset-7 /v/ci/travis/build-docker.sh";
else
  build_script="/v/ci/travis/build-docker.sh";
fi

# TEST_INSTALL=yes builds work better as root, but other builds should avoid
# creating root-owned files in the build directory.
docker_uid="${UID:-0}"
docker_home=""
if [ "${TEST_INSTALL:-}" = "yes" -o "${SCAN_BUILD:-}" = "yes" ]; then
  docker_uid=0
  docker_home=/root
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
     --env CHECK_ABI="${CHECK_ABI:-}" \
     --env CHECK_STYLE="${CHECK_STYLE:-}" \
     --env SCAN_BUILD="${SCAN_BUILD:-}" \
     --env GENERATE_DOCS="${GENERATE_DOCS:-}" \
     --env TEST_INSTALL="${TEST_INSTALL:-}" \
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
     ${build_script}
