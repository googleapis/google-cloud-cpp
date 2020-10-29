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

source "$(dirname "$0")/../../lib/init.sh"
source module etc/repo-config.sh
source module lib/io.sh

export CC=gcc
export CXX=g++
export DISTRO=fedora-install
export DISTRO_VERSION=33
export CMAKE_SOURCE_DIR="."

export GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS="yes"

# Define the build tool so other scripts (e.g. cache download),
# currently the possible values are `CMake` or `Bazel`.
export BUILD_TOOL="CMake"

in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"

if [[ $# -ge 1 ]]; then
  export BUILD_NAME="${1}"
elif [[ -n "${BUILD_NAME:-}" ]]; then
  echo "Using BUILD_NAME=${BUILD_NAME} from environment."
elif [[ -n "${KOKORO_JOB_NAME:-}" ]]; then
  # Kokoro injects the KOKORO_JOB_NAME environment variable, the value of this
  # variable is cloud-cpp/spanner/<config-file-name-without-cfg> (or more
  # generally <path/to/config-file-without-cfg>). By convention we name these
  # files `$foo.cfg` for continuous builds and `$foo-presubmit.cfg` for
  # presubmit builds. Here we extract the value of "foo" and use it as the build
  # name.
  BUILD_NAME="$(basename "${KOKORO_JOB_NAME}" "-presubmit")"
  export BUILD_NAME

  # This is passed into the environment of the docker build and its scripts to
  # tell them if they are running as part of a CI build rather than just a
  # human invocation of "build.sh <build-name>". This allows scripts to be
  # strict when run in a CI, but a little more friendly when run by a human.
  RUNNING_CI="yes"
  export RUNNING_CI
else
  echo "Aborting build as the build name is not defined."
  echo "If you are invoking this script via the command line use:"
  echo "    $0 <build-name>"
  echo
  echo "If this script is invoked by Kokoro, the CI system is expected to set"
  echo "the KOKORO_JOB_NAME environment variable."
  exit 1
fi

# This variable can optionally hold a comma-separated list of words indicating
# which of Spanner's really slow integration tests we want to enable. For
# example, "instance", or "backup", or "instance,backup". These tests can be
# quite slow and so we sometimes selectively enable them. By default, none are
# run.
export GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS

# RUN_INTEGRATION_TESTS has three possible values: "yes", "no", and "auto".
# "auto" runs the integration tests only when the configuration file is present;
# this makes it convenient to run locally, where we may or may not have the
# configuration files present and would rather skip some tests than fail.
#
# If RUN_INTEGRATION_TESTS is set in the environment, we should generally use
# that value, but individual builds can override it if necessary.
if [[ "${BUILD_NAME}" = "clang-tidy" ]]; then
  # Compile with clang-tidy(1) turned on. The build treats clang-tidy warnings
  # as errors.
  export DISTRO=fedora-install
  export DISTRO_VERSION=33
  export CC=clang
  export CXX=clang++
  export BUILD_TYPE=Debug
  export CHECK_STYLE=yes
  export CHECK_MARKDOWN=yes
  export GENERATE_DOCS=yes
  export CLANG_TIDY=yes
  export TEST_INSTALL=yes
  : "${RUN_INTEGRATION_TESTS:=no}" # Don't run integration tests by default.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "coverage" ]]; then
  export DISTRO=fedora-install
  export DISTRO_VERSION=33
  # TODO(4306): Enable the backup tests once they don't timeout too often.
  GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS="instance"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "integration" ]]; then
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  RUN_INTEGRATION_TESTS="yes" # Integration tests were explicitly requested.
  # TODO(4306): Enable the backup tests once they don't timeout too often.
  GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS="instance"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "integration-nightly" ]]; then
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  RUN_INTEGRATION_TESTS="yes" # Integration tests were explicitly requested.
  ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS="yes"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "publish-refdocs" ]]; then
  export BUILD_TYPE=Debug
  export CC=clang
  export CXX=clang++
  export GENERATE_DOCS=yes
  RUN_INTEGRATION_TESTS="no" # Never run integration tests.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "asan" ]]; then
  # Compile with the AddressSanitizer enabled.
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora
  export DISTRO_VERSION=33
  export BAZEL_CONFIG="asan"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "msan" ]]; then
  # Compile with the MemorySanitizer enabled.
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora-libcxx-msan
  export DISTRO_VERSION=33
  export BAZEL_CONFIG="msan"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "tsan" ]]; then
  # Compile with the ThreadSanitizer enabled.
  # We need to use clang-9 for the TSAN build because:
  #   https://github.com/google/sanitizers/issues/1259
  # Using Fedora >= 32 will push us to clang-10, and Fedora < 32 is EOL.
  # Fortunately, Ubuntu:18.04 is a LTS release with clang-9 as an alternative.
  export CC=clang-9
  export CXX=clang++-9
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  export BAZEL_CONFIG="tsan"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "ubsan" ]]; then
  # Compile with the UndefinedBehaviorSanitizer enabled.
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora
  export DISTRO_VERSION=33
  export BAZEL_CONFIG="ubsan"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "cmake-super" ]]; then
  export CMAKE_SOURCE_DIR="super"
  export BUILD_TYPE=Release
  export CMAKE_FLAGS="-DBUILD_SHARED_LIBS=yes -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  RUN_INTEGRATION_TESTS="no" # super builds do not support integration tests.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "gcc-9" ]]; then
  # Compile under fedora:31. This distro uses gcc-9.
  export DISTRO=fedora-install
  export DISTRO_VERSION=33
  export CC=gcc
  export CXX=g++
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "clang-9" ]]; then
  # Compile under fedora:31. This distro uses clang-9.
  export DISTRO=fedora-install
  export DISTRO_VERSION=33
  export CC=clang
  export CXX=clang++
elif [[ "${BUILD_NAME}" = "noex" ]]; then
  # Compile with -fno-exceptions
  export CMAKE_FLAGS="-DGOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS=no"
elif [[ "${BUILD_NAME}" = "no-tests" ]]; then
  # Verify that the code can be compiled without unit tests. This is helpful for
  # package maintainers, where the cost of running the tests for a fixed version
  # is too high.
  export BUILD_TESTING=no
elif [[ "${BUILD_NAME}" = "gcc-5.4" ]]; then
  # The oldest version of GCC we support is 5.4, this build checks the code
  # against that version. The use of Ubuntu 16.04 for that build is not a
  # coincidence: the reason we support GCC 5.4 is to support this distribution.
  # See also https://github.com/googleapis/google-cloud-cpp/issues/4788
  export DISTRO=ubuntu
  export DISTRO_VERSION=16.04
  export CC=gcc
  export CXX=g++
  export CMAKE_SOURCE_DIR="super"
  export CMAKE_FLAGS="-DBUILD_SHARED_LIBS=yes -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
  RUN_INTEGRATION_TESTS="no" # super builds do not support integration tests.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "clang-3.8" ]]; then
  # The oldest version of Clang we actively test is 3.8. There is nothing
  # particularly interesting about that version. It is simply the version
  # included with Ubuntu:16.04, and the oldest version tested by
  # google-cloud-cpp.
  export DISTRO=ubuntu
  export DISTRO_VERSION=16.04
  export CC=clang
  export CXX=clang++
  export CMAKE_SOURCE_DIR="super"
  export CMAKE_FLAGS="-DBUILD_SHARED_LIBS=yes -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
  RUN_INTEGRATION_TESTS="no" # super builds do not support integration tests.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "libcxx" ]]; then
  # Compile using libc++. This is easier to install on Fedora.
  export CC=clang
  export CXX=clang++
  export DISTRO=fedora-install
  export DISTRO_VERSION=33
  export BAZEL_CONFIG="libcxx"
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
elif [[ "${BUILD_NAME}" = "shared" ]]; then
  # Compile with shared libraries. Needs to have the dependencies pre-installed.
  export CMAKE_FLAGS=-DBUILD_SHARED_LIBS=ON
  export TEST_INSTALL=yes
  export BUILD_TYPE=Debug
elif [[ "${BUILD_NAME}" = "gcs-no-grpc" ]]; then
  # The BUILD_NAME "gcs-no-grpc" triggers some extra cmake flags in the
  # in-docker script.
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "check-abi" ]] || [[ "${BUILD_NAME}" = "update-abi" ]]; then
  export CHECK_ABI=yes
  export TEST_INSTALL=yes
  export CMAKE_FLAGS=-DBUILD_SHARED_LIBS=yes
  export BUILD_TYPE=Debug
  if [[ "${BUILD_NAME}" = "update-abi" ]]; then
    export UPDATE_ABI=yes
  fi
  : "${RUN_INTEGRATION_TESTS:=no}" # Don't run integration tests by default.
elif [[ "${BUILD_NAME}" = "cxx17" ]]; then
  export GOOGLE_CLOUD_CPP_CXX_STANDARD=17
  export TEST_INSTALL=yes
  export DISTRO=fedora-install
  export DISTRO_VERSION=31
  in_docker_script="ci/kokoro/docker/build-in-docker-cmake.sh"
elif [[ "${BUILD_NAME}" = "quickstart-bazel" ]]; then
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  RUN_INTEGRATION_TESTS="no" # quickstart does not support integration tests.
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-quickstart-bazel.sh"
elif [[ "${BUILD_NAME}" = "quickstart-cmake" ]]; then
  export DISTRO=ubuntu
  export DISTRO_VERSION=18.04
  RUN_INTEGRATION_TESTS="no" # quickstart does not support integration tests.
  in_docker_script="ci/kokoro/docker/build-in-docker-quickstart-cmake.sh"
elif [[ "${BUILD_NAME}" = "gcs-grpc" ]]; then
  # Test if GCS over gRPC works.
  export DISTRO=fedora
  export DISTRO_VERSION=31
  # Integration tests were explicitly requested.
  RUN_INTEGRATION_TESTS="yes"
  GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG=media # Test gRPC data plane
  # Run all GCS tests on prod - the emulator doesn't support gRPC
  FORCE_TEST_IN_PRODUCTION="storage"
  GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=""
  export BUILD_TOOL="Bazel"
  in_docker_script="ci/kokoro/docker/build-in-docker-bazel.sh"
else
  echo "Unknown BUILD_NAME (${BUILD_NAME}). Fix the Kokoro .cfg file."
  exit 1
fi

# If RUN_INTEGRATION_TESTS wasn't set in the environment or by one of the
# builds above, default it to "yes" for CI builds and "auto" otherwise.
if [[ -z "${RUN_INTEGRATION_TESTS:-}" ]]; then
  if [[ "${RUNNING_CI:-}" == "yes" ]]; then
    RUN_INTEGRATION_TESTS="yes"
  else
    RUN_INTEGRATION_TESTS="auto"
  fi
fi
export RUN_INTEGRATION_TESTS

echo "================================================================"
io::log_yellow "change working directory to project root."
cd "${PROJECT_ROOT}"

echo "================================================================"
io::log_yellow "Capture Docker version to troubleshoot."
sudo docker version

if [[ -f "${KOKORO_GFILE_DIR:-}/gcr-configuration.sh" ]]; then
  echo "================================================================"
  io::log_yellow "Load Google Container Registry configuration parameters."
  source "${KOKORO_GFILE_DIR:-}/gcr-configuration.sh"
fi

source "${PROJECT_ROOT}/ci/kokoro/define-docker-variables.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

echo "================================================================"
io::log_yellow "Building with ${NCPU} cores on ${PWD}."

echo "================================================================"
io::log_yellow "Setup Google Container Registry access."
if [[ -f "${KOKORO_GFILE_DIR:-}/gcr-service-account.json" ]]; then
  gcloud auth activate-service-account --key-file \
    "${KOKORO_GFILE_DIR}/gcr-service-account.json"
fi

if [[ "${RUNNING_CI:-}" == "yes" ]]; then
  gcloud auth configure-docker
fi

echo "================================================================"
io::log_yellow "Download existing image (if available) for ${DISTRO}."
has_cache="false"
if [[ -n "${PROJECT_ID:-}" ]] && docker pull "${IMAGE}:latest"; then
  echo "Existing image successfully downloaded."
  has_cache="true"
fi

echo "================================================================"
io::log_yellow "Build Docker image ${IMAGE} with development tools for ${DISTRO}."
update_cache="false"

docker_build_flags=(
  # Create the image with the same tag as the cache we are using, so we can
  # upload it.
  "-t" "${IMAGE}:latest"
  "--build-arg" "NCPU=${NCPU}"
  "--build-arg" "DISTRO_VERSION=${DISTRO_VERSION}"
  "-f" "ci/kokoro/docker/Dockerfile.${DISTRO}"
)

if "${has_cache}"; then
  docker_build_flags+=("--cache-from=${IMAGE}:latest")
fi

if [[ "${RUNNING_CI:-}" == "yes" ]] &&
  [[ -z "${KOKORO_GITHUB_PULL_REQUEST_NUMBER:-}" ]]; then
  docker_build_flags+=("--no-cache")
fi

echo "================================================================"
io::log_yellow "Creating Docker image with all the development tools."
echo "    docker build ${docker_build_flags[*]} ci"
io::log_yellow "Logging to ${BUILD_OUTPUT}/create-build-docker-image.log"
# We do not want to print the log unless there is an error, so disable the -e
# flag. Later, we will want to print out the emulator(s) logs *only* if there
# is an error, so disabling from this point on is the right choice.
set +e
mkdir -p "${BUILD_OUTPUT}"
if timeout 3600s docker build "${docker_build_flags[@]}" ci \
  >"${BUILD_OUTPUT}/create-build-docker-image.log" 2>&1 </dev/null; then
  update_cache="true"
  io::log_green "Docker image successfully rebuilt"
else
  io::log_yellow "Error updating Docker image, using cached image for this build"
  dump_log "${BUILD_OUTPUT}/create-build-docker-image.log"
fi

if "${update_cache}" && [[ "${RUNNING_CI:-}" == "yes" ]] &&
  [[ -z "${KOKORO_GITHUB_PULL_REQUEST_NUMBER:-}" ]]; then
  echo "================================================================"
  io::log_yellow "Uploading updated base image for ${DISTRO}."
  # Do not stop the build on a failure to update the cache.
  docker push "${IMAGE}:latest" || true
fi

# Because Kokoro checks out the code in `detached HEAD` mode there is no easy
# way to discover what is the current branch (and Kokoro does not expose the
# branch as an enviroment variable, like other CI systems do). We use the
# following trick:
# - Find out the current commit using git rev-parse HEAD.
# - Exclude "HEAD detached" branches (they are not really branches).
# - Choose the branch from the bottom of the list.
# - Typically this is the branch that was checked out by Kokoro.
echo "================================================================"
io::log_yellow "Detecting the branch name."
BRANCH="$(git branch --all --no-color --contains "$(git rev-parse HEAD)" |
  grep -v 'HEAD' | tail -1 || exit 0)"
# Enable extglob if not enabled
shopt -q extglob || shopt -s extglob
BRANCH="${BRANCH##* }"
BRANCH="${BRANCH%%* }"
BRANCH="${BRANCH##remotes/origin/}"
BRANCH="${BRANCH##remotes/upstream/}"
export BRANCH
io::log_green "Detected the branch name: ${BRANCH}."

# The default user for a Docker container has uid 0 (root). To avoid creating
# root-owned files in the build directory we tell docker to use the current
# user ID.
docker_uid="$(id -u)"
docker_gid="$(id -g)"
docker_user="$(id -un)"
docker_home_prefix="${PWD}/cmake-out/home"
if [[ "${docker_uid}" == "0" ]]; then
  # If the UID is 0, then the HOME directory will be set to /root, and we
  # need to mount the ccache files is /root/.ccache.
  docker_home_prefix="${PWD}/cmake-out/root"
fi

mkdir -p "${BUILD_HOME}"

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

  "--env" "NCPU=${NCPU}"

  # Pass down the BUILD_NAME.
  "--env" "BUILD_NAME=${BUILD_NAME}"
  "--env" "BRANCH=${BRANCH}"

  # If set, pass -DGOOGLE_CLOUD_CPP_CXX_STANDARD=<value> to CMake.
  "--env" "GOOGLE_CLOUD_CPP_CXX_STANDARD=${GOOGLE_CLOUD_CPP_CXX_STANDARD:-}"

  # The type of the build for CMake.
  "--env" "BUILD_TYPE=${BUILD_TYPE:-Release}"
  # Additional flags to enable CMake features.
  "--env" "CMAKE_FLAGS=${CMAKE_FLAGS:-}"

  # When running the integration tests this directory contains the
  # configuration files needed to run said tests. Make it available inside
  # the Docker container.
  "--volume" "${KOKORO_GFILE_DIR:-/dev/shm}:/c"

  # The type of the build for Bazel.
  "--env" "BAZEL_CONFIG=${BAZEL_CONFIG:-}"

  # With CMake we can disable the tests, this is useful for package
  # maintainers and we need to test it.
  "--env" "BUILD_TESTING=${BUILD_TESTING:=yes}"

  # If set, enable using libc++ with CMake.
  "--env" "USE_LIBCXX=${USE_LIBCXX:-}"

  # If set, run the check-abi.sh script.
  "--env" "CHECK_ABI=${CHECK_ABI:-}"

  # If set, run the check-abi.sh script and *then* update the API/ABI
  # baseline.
  "--env" "UPDATE_ABI=${UPDATE_ABI:-}"

  # If set, run the scripts to check (and fix) the code formatting (i.e.
  # clang-format, cmake-format, and buildifier).
  "--env" "CHECK_STYLE=${CHECK_STYLE:-}"

  # If set to 'yes', the build script will verify that auto-generated
  # markdown files are in sync.
  "--env" "CHECK_MARKDOWN=${CHECK_MARKDOWN:-}"

  # If set to 'yes', the build script will configure clang-tidy. Currently
  # only the CMake builds use this flag.
  "--env" "CLANG_TIDY=${CLANG_TIDY:-}"

  # If set to 'no', skip the integration tests.
  "--env" "RUN_INTEGRATION_TESTS=${RUN_INTEGRATION_TESTS:-}"

  # Comma-separated list of components whose integration tests should be run
  # against production even if they're tagged as for the emulator.
  "--env" "FORCE_TEST_IN_PRODUCTION=${FORCE_TEST_IN_PRODUCTION:-}"

  # GCS tests may use gRPC for parts of the protocol. This variable controls it.
  # Setting it to 'media' will use gRPC for the data plane. Setting it to
  # 'metadata' will use gRPC for everything.
  "--env" "GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG=${GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG:-}"

  # The Bigtable Admin integration tests can only run in nightly builds, the
  # quota (as in calls per day) is too restrictive to run more often.
  "--env" "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS=${ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS:-no}"

  # If set, executes the generator integration tests.
  "--env" "GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS:-no}"

  # These tests are quite slow, so we only enable them in certain builds.
  "--env" "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS:-}"

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

  # Configure the location of the Cloud SDK.
  "--env" "CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk"

  # Let the Docker image script know what kind of terminal we are using, that
  # produces properly colorized error messages.
  "--env" "TERM=${TERM:-dumb}"

  # If set, add a flag --runs_per_test=<value> to bazel or --repeat-until-fail
  # to ctest.
  "--env" "RUNS_PER_TEST=${RUNS_PER_TEST:-}"

  # Tells scripts whether they are running as part of a CI or not.
  "--env" "RUNNING_CI=${RUNNING_CI:-no}"

  # Run the docker script and this user id. Because the docker image gets to
  # write in ${PWD} you typically want this to be your user id.
  "--user" "${docker_uid}:${docker_gid}"

  # Bazel needs this environment variable to work correctly.
  "--env" "USER=${docker_user}"

  # We give Bazel and CMake a fake $HOME inside the docker image. Bazel caches
  # build byproducts in this directory. CMake (when ccache is enabled) uses
  # it to store $HOME/.ccache

  # Make the fake directory available inside the docker image as `/h`.
  "--volume" "${PWD}/${BUILD_HOME}:/h"
  "--env" "HOME=/h"

  # Mount the current directory (which is the top-level directory for the
  # project) as `/v` inside the docker image, and move to that directory.
  "--volume" "${PWD}:/v"
  "--workdir" "/v"

  # Mask any other builds that may exist at the same time. That is, these
  # directories appear as empty inside the Docker container, this prevents the
  # container from writing into other builds, or to get confused by the output
  # of other builds. In the CI system this does not matter, as each build runs
  # on a completely separate VM. This is useful when running multiple builds
  # in your workstation.
  "--volume" "/v/cmake-out/home"
  "--volume" "/v/cmake-out"
  "--volume" "${PWD}/${BUILD_OUTPUT}:/v/${BUILD_OUTPUT}"

  # No need to preserve the container.
  "--rm"
)

if [[ -n "${KOKORO_JOB_TYPE:-}" ]]; then
  docker_flags+=("--env" "KOKORO_JOB_TYPE=${KOKORO_JOB_TYPE:-}")
fi

if [[ -n "${KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH:-}" ]]; then
  docker_flags+=("--env" "KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH=${KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH:-}")
fi

if [[ -n "${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT:-}" ]]; then
  docker_flags+=(
    "--env" "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT=${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT}"
  )
fi

# Optionally allow the docker container to use the host's networking in order
# to take advantage of potentially non-trivial network configurations (e.g.,
# DNS cache) that might be a PITA to duplicate within the container itself.
if [[ -n "${DOCKER_USE_HOST_NETWORK:-}" ]]; then
  docker_flags+=("--net" "host")
fi

# When running on Travis the build gets a tty, and docker can produce nicer
# output in that case, but on Kokoro the script does not get a tty, and Docker
# terminates the program if we pass the `-it` flag in that case.
if [[ -t 0 ]]; then
  docker_flags+=("-it")
fi

CACHE_BUCKET="${GOOGLE_CLOUD_CPP_KOKORO_RESULTS:-cloud-cpp-kokoro-results}"
readonly CACHE_BUCKET
CACHE_FOLDER="${CACHE_BUCKET}/build-cache/${GOOGLE_CLOUD_CPP_REPOSITORY}"
readonly CACHE_FOLDER
CACHE_NAME="cache-${DOCKER_IMAGE_BASENAME}-${BUILD_NAME}"
readonly CACHE_NAME

echo "================================================================"
io::log_yellow "Downloading build cache."

"${PROJECT_ROOT}/ci/kokoro/docker/download-cache.sh" \
  "${CACHE_FOLDER}" "${CACHE_NAME}" "${BUILD_HOME}" || true

# If more than two arguments are given, arguments after the first one will
# become the commands run in the container, otherwise run $in_docker_script with
# appropriate arguments.
echo "================================================================"
if [[ $# -ge 2 ]]; then
  io::log_yellow "Running the given commands '" "${@:2}" "' in the container."
  readonly commands=("${@:2}")
else
  io::log_yellow "Running the full build inside docker."
  readonly commands=(
    "/v/${in_docker_script}"
    "${CMAKE_SOURCE_DIR}"
    "${BUILD_OUTPUT}"
  )
fi

# Run the docker image with that giant collection of flags.
echo sudo docker run "${docker_flags[@]}" "${IMAGE}:latest" "${commands[@]}"
sudo docker run "${docker_flags[@]}" "${IMAGE}:latest" "${commands[@]}"

exit_status=$?
if [[ "${exit_status}" -eq 0 ]]; then
  io::log_green "Docker run succeeded."
else
  io::log_red "Docker run failed with exit status ${exit_status}."
fi

if [[ "${BUILD_NAME}" == "publish-refdocs" ]]; then
  "${PROJECT_ROOT}/ci/kokoro/docker/publish-refdocs.sh"
  exit_status=$?
elif [[ "${RUNNING_CI:-}" == "yes" ]]; then
  # Note that only the `clang-tidy` build config contains the token needed
  # to actually upload the docs, otherwise this has no effect.
  "${PROJECT_ROOT}/ci/kokoro/docker/upload-docs.sh" "${BRANCH}" "${BUILD_OUTPUT}"
fi

"${PROJECT_ROOT}/ci/kokoro/docker/upload-coverage.sh" \
  "${IMAGE}:latest" "${docker_flags[@]}"

if [[ "${exit_status}" -eq 0 ]]; then
  "${PROJECT_ROOT}/ci/kokoro/docker/upload-cache.sh" \
    "${CACHE_FOLDER}" "${CACHE_NAME}" "${BUILD_HOME}" || true
else
  echo "================================================================"
  io::log_yellow "Build failed; printing logs."
  "${PROJECT_ROOT}/ci/kokoro/docker/dump-logs.sh"
fi

echo "================================================================"
io::log_yellow "Dumping API/ABI compatibility reports."
"${PROJECT_ROOT}/ci/kokoro/docker/dump-reports.sh"

echo "================================================================"
if [[ "${RUNNING_CI:-}" == "yes" ]] && [[ -n "${KOKORO_ARTIFACTS_DIR:-}" ]]; then
  # Our CI system (kokoro) syncs the data in this directory to somewhere after
  # the build completes. Removing the cmake-out/ dir shaves minutes off this
  # process. This is safe as long as we don't wish to save any build artifacts.
  io::log_yellow "cleaning up artifacts."
  find "${KOKORO_ARTIFACTS_DIR}" -name cmake-out -type d -prune -print0 |
    xargs -0 -t rm -rf
else
  io::log_yellow "Not a CI build; skipping artifact cleanup"
fi

echo
echo "================================================================"
if [[ ${exit_status} -eq 0 ]]; then
  io::log_green "Build script finished successfully."
else
  io::log_red "Build script failed with status ${exit_status}"
fi

exit ${exit_status}
