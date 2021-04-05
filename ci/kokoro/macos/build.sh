#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
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
source module /ci/etc/repo-config.sh
source module /ci/lib/io.sh

export BAZEL_CONFIG=""
export RUN_INTEGRATION_TESTS="no"
export BUILD_TOOL="CMake"
driver_script="ci/kokoro/macos/build-bazel.sh"

# TODO(#4896): Enable generator integration tests for macos.
export GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS="no"

if [[ $# -eq 1 ]]; then
  export BUILD_NAME="${1}"
elif [[ -n "${KOKORO_JOB_NAME:-}" ]]; then
  # Kokoro injects the KOKORO_JOB_NAME environment variable, the value of this
  # variable is cloud-cpp/pubsub/<config-file-name-without-cfg> (or more
  # generally <path/to/config-file-without-cfg>). By convention we name these
  # files `$foo.cfg` for continuous builds and `$foo-presubmit.cfg` for
  # presubmit builds. Here we extract the value of "foo" and use it as the build
  # name.
  BUILD_NAME="$(basename "${KOKORO_JOB_NAME}" "-presubmit")"
  export BUILD_NAME

  # This is passed into the environment of the remaining scripts to tell them if
  # they are running as part of a CI build rather than just a human invocation
  # of "build.sh <build-name>". This allows scripts to be strict when run in a
  # CI, but a little more friendly when run by a human.
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

# In some versions of Kokoro `google-cloud-sdk` is not installed by default, or
# it is just partially installed. This gives us a more consistent environment.
echo "================================================================"
io::log_yellow "Update or reinstall 'google-cloud-sdk'."
brew --version
# Ignore errors, maybe the local version is functional.
env "HOMEBREW_NO_AUTO_UPDATE=1" brew reinstall google-cloud-sdk ||
  env "HOMEBREW_NO_AUTO_UPDATE=1" brew cask install google-cloud-sdk ||
  true

# Continue despite `brew doctor` errors and warnings.
env "HOMEBREW_NO_AUTO_UPDATE=1" brew doctor || true

echo "================================================================"
io::log_yellow "change working directory to project root."
cd "${PROJECT_ROOT}"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

echo "================================================================"
io::log_yellow "building with ${NCPU} cores on ${PWD}."

script_flags=()

if [[ "${BUILD_NAME}" == "bazel" ]]; then
  export BUILD_TOOL="Bazel"
  driver_script="ci/kokoro/macos/build-bazel.sh"
elif [[ "${BUILD_NAME}" == "cmake-super" ]]; then
  driver_script="ci/kokoro/macos/build-cmake.sh"
  script_flags+=("super" "cmake-out/macos")
elif [[ "${BUILD_NAME}" == "quickstart-cmake" ]]; then
  driver_script="ci/kokoro/macos/build-quickstart-cmake.sh"
elif [[ "${BUILD_NAME}" == "quickstart-bazel" ]]; then
  export BUILD_TOOL="Bazel"
  driver_script="ci/kokoro/macos/build-quickstart-bazel.sh"
else
  io::log_red "unknown BUILD_NAME (${BUILD_NAME})."
  exit 1
fi

# Sadly, the Kokoro machines seem to be out of sync
if [[ "${RUNNING_CI:-}" != "yes" ]]; then
  io::log_yellow "Skipping clock sync for interactive build"
elif type ntpdate >/dev/null 2>&1; then
  echo "================================================================"
  io::log_yellow "using ntpdate to synchronize clock from time.google.com."
  sudo ntpdate time.google.com
elif type sntp >/dev/null 2>&1; then
  echo "================================================================"
  io::log_yellow "using sntp to synchronize clock from time.google.com."
  sudo sntp -sS time.google.com
else
  echo "================================================================"
  io::log_red "no command available to sync clock"
fi

# We need this environment variable because on macOS gRPC crashes if it cannot
# find the credentials, even if you do not use them. Some of the unit tests do
# exactly that.
echo "================================================================"
io::log_yellow "define GOOGLE_APPLICATION_CREDENTIALS."
export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/kokoro-run-key.json"

# Download the gRPC `roots.pem` file. On macOS gRPC does not use the native
# trust store. One needs to set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH. There was
# a PR to fix this:
#    https://github.com/grpc/grpc/pull/16246
# But it was closed without being merged, and there are open bugs:
#    https://github.com/grpc/grpc/issues/16571
echo "================================================================"
io::log_yellow "getting roots.pem for gRPC."
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="${KOKORO_GFILE_DIR}/roots.pem"
rm -f "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
curl -sSL --retry 10 -o "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}" \
  https://pki.google.com/roots.pem

BRANCH="${KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH:-master}"
readonly BRANCH
echo "================================================================"
io::log_yellow "detected the branch name: ${BRANCH}."

CACHE_BUCKET="${GOOGLE_CLOUD_CPP_KOKORO_RESULTS:-cloud-cpp-kokoro-results}"
readonly CACHE_BUCKET
CACHE_FOLDER="${CACHE_BUCKET}/build-cache/${GOOGLE_CLOUD_CPP_REPOSITORY}"
CACHE_FOLDER="${CACHE_FOLDER}/${BRANCH}"
readonly CACHE_FOLDER
CACHE_NAME="cache-macos-${BUILD_NAME}"
readonly CACHE_NAME

echo "================================================================"
gtimeout 1200 "${PROJECT_ROOT}/ci/kokoro/macos/download-cache.sh" \
  "${CACHE_FOLDER}" "${CACHE_NAME}" || true

echo "================================================================"
io::log_yellow "starting build script."

if "${driver_script}" "${script_flags[@]+"${script_flags[@]}"}"; then
  io::log_green "build script was successful."
else
  io::log_red "build script reported errors."
  exit 1
fi

io::log_yellow "post build cache upload + cleanup"

gtimeout 1200 "${PROJECT_ROOT}/ci/kokoro/macos/upload-cache.sh" \
  "${CACHE_FOLDER}" "${CACHE_NAME}" || true

if [[ "${RUNNING_CI:-}" == "yes" ]] && [[ -n "${KOKORO_ARTIFACTS_DIR:-}" ]]; then
  # Our CI system (Kokoro) syncs the data in this directory to somewhere after
  # the build completes. Removing the cmake-out/ dir shaves minutes off this
  # process. This is safe as long as we don't wish to save any build artifacts.
  io::log_yellow "cleaning up artifacts."
  find "${KOKORO_ARTIFACTS_DIR}" -name cmake-out -type d -prune -print0 |
    xargs -0 -t rm -rf || true
else
  io::log_yellow "Not a CI build; skipping artifact cleanup"
fi

exit 0
