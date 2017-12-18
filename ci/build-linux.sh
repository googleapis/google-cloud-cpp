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
sudo docker build -t "${IMAGE}:tip" \
     --build-arg DISTRO_VERSION="${DISTRO_VERSION}" \
     --build-arg CXX="${CXX}" \
     --build-arg CC="${CC}" \
     --build-arg NCPU="${NCPU:-2}" \
     --build-arg BUILD_TYPE="${BUILD_TYPE:-Release}" \
     --build-arg CHECK_STYLE="${CHECK_STYLE:-}" \
     --build-arg SCAN_BUILD="${SCAN_BUILD:-}" \
     --build-arg GENERATE_DOCS="${GENERATE_DOCS:-}" \
     --build-arg CMAKE_FLAGS="${CMAKE_FLAGS:-}" \
     -f "ci/Dockerfile.${DISTRO}" .

if [ "${SCAN_BUILD:-}" = "yes" ]; then
    sudo docker run --rm -it --volume $PWD:/d ${IMAGE}:tip \
	 bash -c 'if [ ! -z "$(ls -1d /tmp/scan-build-* 2>/dev/null)" ]; then cp -r /tmp/scan-build-* /d/scan-build-output; fi'
    if [ -r scan-build-output/index.html ]; then
        echo -n $(tput setaf 1)
	echo "scan-build detected errors.  Please read the log for details."
	echo "To run scan-build locally, and examine the html output please"
	echo "install and configure Docker, then run:"
        echo
        echo "DISTRO=ubuntu DISTRO_VERSION=17.04 SCAN_BUILD=yes NCPU=8 TRAVIS_OS_NAME=linux CXX=clang++ CC=clang ./ci/build-linux.sh"
        echo
        echo "the html output will be copied into the scan-build-output"
	echo "subdirectory."
        exit 1
    else
        echo -n $(tput setaf 2)
        echo "scan-build completed without errors."
        echo -n $(tput sgr0)
    fi
fi
