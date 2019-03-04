#!/usr/bin/env bash
#
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

set -eu

export CC=gcc
export CXX=g++
export DISTRO=ubuntu
export DISTRO_VERSION=18.04

if [[ "${BUILD_NAME+x}" != "x" ]]; then
 echo "The BUILD_NAME is not defined or is empty. Fix the Kokoro .cfg file."
 exit 1
elif [[ "${BUILD_NAME}" = "asan" ]]; then
  # Compile with the AddressSanitizer enabled.
  export BUILD_TYPE=Debug
  export CC=clang
  export CXX=clang++
  export CMAKE_FLAGS="-DSANITIZE_ADDRESS=yes"
elif [[ "${BUILD_NAME}" = "centos-7" ]]; then
  # Compile under centos:7. This distro uses gcc-4.8.
  export DISTRO=centos
  export DISTRO_VERSION=7
elif [[ "${BUILD_NAME}" = "noex" ]]; then
  # Compile with -fno-exceptions
  export DISTRO_VERSION=16.04
  export CMAKE_FLAGS="-DGOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=no"
elif [[ "${BUILD_NAME}" = "ubsan" ]]; then
  # Compile with the UndefinedBehaviorSanitizer enabled.
  export BUILD_TYPE=Debug
  export CC=clang
  export CXX=clang++
  export CMAKE_FLAGS="-DSANITIZE_UNDEFINED=yes"
elif [[ "${BUILD_NAME}" = "clang-tidy" ]]; then
  # Compile with clang-tidy(1) turned on. The build treats clang-tidy warnings
  # as errors.
  export BUILD_TYPE=Debug
  export CC=clang
  export CXX=clang++
  export CMAKE_FLAGS="-DGOOGLE_CLOUD_CPP_CLANG_TIDY=yes"
elif [[ "${BUILD_NAME}" = "libcxx" ]]; then
  # Compile using libc++. This is easier to install on Fedora.
  export CC=clang
  export CXX=clang++
  export CMAKE_FLAGS=-DBUILD_SHARED_LIBS=ON
  export DISTRO=fedora
  export DISTRO_VERSION=29
  export USE_LIBCXX=yes
elif [[ "${BUILD_NAME}" = "shared" ]]; then
  # Compile with shared libraries. Needs to have the dependencies pre-installed.
  export CMAKE_FLAGS=-DBUILD_SHARED_LIBS=ON
  export TEST_INSTALL=yes
  export BUILD_TYPE=Debug
  export DISTRO=ubuntu-install
  export CHECK_STYLE=yes
  export GENERATE_DOCS=yes
elif [[ "${BUILD_NAME}" = "no-tests" ]]; then
  # Verify that the code can be compiled without unit tests. This is helpful for
  # package maintainers, where the cost of running the tests for a fixed version
  # is too high.
  export BUILD_TESTING=no
  export CMAKE_FLAGS=-DBUILD_TESTING=OFF
else
  echo "Unknown BUILD_NAME (${BUILD_NAME}). Fix the Kokoro .cfg file."
  exit 1
fi

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

echo "================================================================"
echo "Updating submodules $(date)."
cd "${PROJECT_ROOT}"
git submodule update --init
echo "================================================================"

echo "================================================================"
export NCPU=$(nproc)
echo "Building with ${NCPU} cores $(date)."

echo "================================================================"
echo "Creating Docker image with all the development tools $(date)."
# We do not want to print the log unless there is an error, so disable the -e
# flag. Later, we will want to print out the emulator(s) logs *only* if there
# is an error, so disabling from this point on is the right choice.
set +e
mkdir -p "${BUILD_OUTPUT}"
"${PROJECT_ROOT}/ci/install-retry.sh" \
    "${PROJECT_ROOT}/ci/travis/install-linux.sh" \
    >"${BUILD_OUTPUT}/create-build-docker-image.log" 2>&1 </dev/null
if [[ "$?" != 0 ]]; then
  dump_log "${BUILD_OUTPUT}/create-build-docker-image.log"
  exit 1
fi
echo "================================================================"

echo "================================================================"
echo "Running the full build $(date)."
export NEEDS_CCACHE=no
"${PROJECT_ROOT}/ci/travis/build-linux.sh"
exit_status=$?
echo "Build finished with ${exit_status} exit status $(date)."
echo "================================================================"

echo "================================================================"
"${PROJECT_ROOT}/ci/travis/dump-logs.sh"
echo "================================================================"

exit ${exit_status}
