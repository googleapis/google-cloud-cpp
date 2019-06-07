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

in_docker_script="ci/travis/build-docker.sh"

if [[ "${BUILD_NAME+x}" != "x" ]]; then
 echo "The BUILD_NAME is not defined or is empty. Fix the Kokoro .cfg file."
 exit 1
elif [[ "${BUILD_NAME}" = "asan" ]]; then
  # Compile with the AddressSanitizer enabled.
  export CC=clang
  export CXX=clang++
  export BAZEL_CONFIG="asan"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "centos-7" ]] || [[ "${BUILD_NAME}" = "gcc-4.8" ]]; then
  # Compile under centos:7. This distro uses gcc-4.8.
  export DISTRO=centos
  export DISTRO_VERSION=7
elif [[ "${BUILD_NAME}" = "gcc-9" ]]; then
  # Compile under fedora:30. This distro uses gcc-9.
  export DISTRO=fedora
  export DISTRO_VERSION=30
elif [[ "${BUILD_NAME}" = "clang-8" ]]; then
  # Compile under fedora:30. This distro uses clang-8.
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora
  export DISTRO_VERSION=30
elif [[ "${BUILD_NAME}" = "noex" ]]; then
  # Compile with -fno-exceptions
  export DISTRO_VERSION=16.04
  export CMAKE_FLAGS="-DGOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=no"
elif [[ "${BUILD_NAME}" = "ubsan" ]]; then
  # Compile with the UndefinedBehaviorSanitizer enabled.
  export CC=clang
  export CXX=clang++
  export BAZEL_CONFIG="ubsan"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
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
  export DISTRO=fedora
  export DISTRO_VERSION=30
  export BAZEL_CONFIG="libcxx"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
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
elif [[ "${BUILD_NAME}" = "check-abi" ]] || [[ "${BUILD_NAME}" = "update-abi" ]]; then
  export CHECK_ABI=yes
  export TEST_INSTALL=yes
  export CMAKE_FLAGS=-DBUILD_SHARED_LIBS=yes
  export BUILD_TYPE=Debug
  export DISTRO=ubuntu-install
  export DISTRO_VERSION=18.04
  if [[ "${BUILD_NAME}" = "update-abi" ]]; then
    export UPDATE_ABI=yes
  fi
elif [[ "${BUILD_NAME}" = "scan-build" ]]; then
  # Compile using Clang's static analyzer. Use fedora because it has recent
  # versions of the toolchain.
  export BUILD_TYPE=Debug
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora-install
  export DISTRO_VERSION=30
  export SCAN_BUILD=yes
else
  echo "Unknown BUILD_NAME (${BUILD_NAME}). Fix the Kokoro .cfg file."
  exit 1
fi

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

echo "================================================================"
NCPU=$(nproc)
export NCPU
cd "${PROJECT_ROOT}"
echo "Building with ${NCPU} cores $(date) on ${PWD}."

echo "================================================================"
echo "Creating Docker image with all the development tools $(date)."
# We do not want to print the log unless there is an error, so disable the -e
# flag. Later, we will want to print out the emulator(s) logs *only* if there
# is an error, so disabling from this point on is the right choice.
set +e
mkdir -p "${BUILD_OUTPUT}"
echo "Logging to ${BUILD_OUTPUT}/create-build-docker-image.log"
"${PROJECT_ROOT}/ci/install-retry.sh" \
    "${PROJECT_ROOT}/ci/travis/install-linux.sh" \
    >"${BUILD_OUTPUT}/create-build-docker-image.log" 2>&1 </dev/null
if [[ "$?" != 0 ]]; then
  dump_log "${BUILD_OUTPUT}/create-build-docker-image.log"
  exit 1
fi
echo "================================================================"

echo "================================================================"
echo "Capture Docker version to troubleshoot $(date)."
sudo docker version
echo "================================================================"

echo "================================================================"
echo "Running the full build inside docker $(date)."
# The default user for a Docker container has uid 0 (root). To avoid creating
# root-owned files in the build directory we tell docker to use the current
# user ID, if known.
docker_uid="${UID:-0}"
docker_user="${USER:-root}"
docker_home_prefix="${PWD}/cmake-out/home"
if [[ "${docker_uid}" == "0" ]]; then
  # If the UID is 0, then the HOME directory will be set to /root, and we
  # need to mount the ccache files is /root/.ccache.
  docker_home_prefix="${PWD}/cmake-out/root"
fi

readonly DOCKER_HOME="${docker_home_prefix}/${IMAGE}${suffix}"
mkdir -p "${DOCKER_HOME}"

# We use an array for the flags so they are easier to document.
docker_flags=(
    # Enable ptrace as it is needed by s
    "--cap-add" "SYS_PTRACE"

    # The name and version of the, this is used to call linux-config.sh
    "--env" "DISTRO=${DISTRO}"
    "--env" "DISTRO_VERSION=${DISTRO_VERSION}"

    # The C++ and C compiler, both Bazel and CMake use this environment variable
    # to select the compiler binary.
    "--env" "CXX=${CXX}"
    "--env" "CC=${CC}"

    # The number of CPUs, probably should be removed, the scripts can detect
    # this themselves in Kokoro (it was a problem on Travis).
    "--env" "NCPU=${NCPU:-4}"

    # Disable ccache(1) for Kokoro build builds we do not cache data between
    # builds.
    "--env" "NEEDS_CCACHE=no"

    # The type of the build for CMake.
    "--env" "BUILD_TYPE=${BUILD_TYPE:-Release}"
    # Additional flags to enable CMake features.
    "--env" "CMAKE_FLAGS=${CMAKE_FLAGS:-}"

    # The type of the build for Bazel.
    "--env" "BAZEL_CONFIG=${BAZEL_CONFIG:-}"

    # With CMake we can disable the tests, this is useful for package
    # maintainers and we need to test it.
    "--env" "BUILD_TESTING=${BUILD_TESTING:=yes}"

    # If set, enable using libc++ with CMake.
    "--env" "USE_LIBCXX=${USE_LIBCXX:-}"

    # If set, use Clang's static analyzer. Currently there is no build that
    # uses this feature, it may have rotten.
    "--env" "SCAN_BUILD=${SCAN_BUILD:-}"

    # If set, run the check-abi.sh script.
    "--env" "CHECK_ABI=${CHECK_ABI:-}"

    # If set, run the check-abi.sh script and *then* update the API/ABI
    # baseline.
    "--env" "UPDATE_ABI=${UPDATE_ABI:-}"

    # If set, run the scripts to check (and fix) the code formatting (i.e.
    # clang-format, cmake-format, and buildifier).
    "--env" "CHECK_STYLE=${CHECK_STYLE:-}"

    # If set, run the scripts to generate Doxygen docs. Note that the scripts
    # to upload said docs are not part of the build, they run afterwards on
    # Travis.
    "--env" "GENERATE_DOCS=${GENERATE_DOCS:-}"

    # If set, execute tests to verify `make install` works and produces working
    # installations.
    "--env" "TEST_INSTALL=${TEST_INSTALL:-}"

    # Configure the location of the Cloud Bigtable command-line tool and
    # emulator.
    "--env" "CBT=/usr/local/google-cloud-sdk/bin/cbt"
    "--env" "CBT_EMULATOR=/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator"

    # Let the Docker image script know what kind of terminal we are using, that
    # produces properly colorized error messages.
    "--env" "TERM=${TERM:-dumb}"

    # Run the docker script and this user id. Because the docker image gets to
    # write in ${PWD} you typically want this to be your user id.
    "--user" "${docker_uid}"

    # Bazel needs this environment variable to work correctly.
    "--env" "USER=${docker_user}"

    # We give Bazel and CMake a fake $HOME inside the docker image. Bazel caches
    # build byproducts in this directory. CMake (when ccache is enabled) uses
    # it to store $HOME/.ccache

    # Make the fake directory available inside the docker image as `/h`.
    "--volume" "${DOCKER_HOME}:/h"
    "--env" "HOME=/h"

    # Mount the current directory (which is the top-level directory for the
    # project) as `/v` inside the docker image, and move to that directory.
    "--volume" "${PWD}:/v"
    "--workdir" "/v"
)

# When running on Travis the build gets a tty, and docker can produce nicer
# output in that case, but on Kokoro the script does not get a tty, and Docker
# terminates the program if we pass the `-it` flag in that case.
if [[ -t 0 ]]; then
  docker_flags+=("-it")
fi

# Run the docker image with that giant collection of flags.
sudo docker run "${docker_flags[@]}" "${IMAGE}:tip" "/v/${in_docker_script}"
exit_status=$?
echo "Build finished with ${exit_status} exit status $(date)."
echo "================================================================"

echo "================================================================"
"${PROJECT_ROOT}/ci/travis/dump-logs.sh"
echo "================================================================"

echo "================================================================"
"${PROJECT_ROOT}/ci/travis/dump-reports.sh"
echo "================================================================"

echo
echo "Build script finished with ${exit_status} exit status $(date)."
echo
exit ${exit_status}
