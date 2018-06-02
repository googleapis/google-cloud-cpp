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

set -eu

function install_dependencies {
  sudo apt-get update
  sudo apt-get install -y software-properties-common
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
  sudo apt-get update
  sudo apt-get install -y \
       gcc-4.9 \
       g++-4.9 \
       libcurl4-openssl-dev \
       libssl-dev \
       unzip \
       wget \
       zip \
       zlib1g-dev

  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 100
  sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.9 100

  readonly BAZEL_VERSION=0.12.0
  readonly GITHUB_DL="https://github.com/bazelbuild/bazel/releases/download"
  wget -q "${GITHUB_DL}/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh"
  chmod +x "bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh"
  ./bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh --user
}

# Make three attempts to install the dependencies. It is rare, but from time to
# time the downloading the packages and/or the Bazel installer fails. To make
# the CI build more robust, try again when that happens.

# Initially, wait at least 3 minutes (the times are in seconds), because it
# makes no sense to try faster.  We currently have 20+ minutes of remaining
# budget to complete a build, and we should be as nice as possible to the
# servers that provide the packages.
min_wait=180
# Do not exit on failures for this loop.
set +e
for i in 1 2 3; do
  install_dependencies
  if [ $? -eq 0 ]; then
    exit 0
  fi
  # Sleep for a few minutes before trying again.
  readonly PERIOD=$[ (${RANDOM} % 60) + min_wait ]
  echo "Fetch failed; trying again in ${PERIOD} seconds."
  sleep ${PERIOD}s
  min_wait=$[ min_wait * 2 ]
done
