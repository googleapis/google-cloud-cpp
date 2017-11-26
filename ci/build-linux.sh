#!/bin/sh
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

set -e

if [ "${TRAVIS_OS_NAME}" != "linux" ]; then
  echo "Not a Linux-based build, exit successfully."
  exit 0
fi

readonly IMAGE="cached-${DISTRO?}-${DISTRO_VERSION?}"
readonly LATEST_ID=$(sudo docker inspect -f '{{ .Id }}' ${IMAGE?}:latest 2>/dev/null || echo "")

echo IMAGE = ${IMAGE}
echo IMAGE LATEST ID = ${LATEST_ID}

# TODO() - on cron buiids, we would want to disable the cache
# altogether, to make sure we can still build against recent versions
# of grpc, protobug, the compilers, etc.
cacheargs=""
if [ -z "${LATEST_ID}" ]; then
  cacheargs="--cache-from ${IMAGE?}:latest"
fi

echo cache args = ${cacheargs}

sudo docker build -t ${IMAGE?}:tip ${cacheargs?} \
     --build-arg DISTRO_VERSION=${DISTRO_VERSION?} \
     --build-arg CXX=${CXX?} \
     --build-arg CC=${CC?} \
     --build-arg TRAVIS_JOB_NUMBER=${TRAVIS_JOB_NUMBER} \
     -f ci/Dockerfile.${DISTRO?} .
