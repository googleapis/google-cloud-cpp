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

export BAZEL_CONFIG=""
export RUN_INTEGRATION_TESTS="no"
driver_script="ci/kokoro/macos/build-bazel.sh"

# Set it to "no" for any value other than "yes".
if [[ "${RUN_SLOW_INTEGRATION_TESTS:-}" != "yes" ]]; then
  RUN_SLOW_INTEGRATION_TESTS="no"
fi
export RUN_SLOW_INTEGRATION_TESTS

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
else
  echo "Aborting build as the build name is not defined."
  echo "If you are invoking this script via the command line use:"
  echo "    $0 <build-name>"
  echo
  echo "If this script is invoked by Kokoro, the CI system is expected to set"
  echo "the KOKORO_JOB_NAME environment variable."
  exit 1
fi

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/ci/etc/repo-config.sh"

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): change working directory to project root."\
    "${COLOR_RESET}"
cd "${PROJECT_ROOT}"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): building with ${NCPU} cores on ${PWD}."\
    "${COLOR_RESET}"

script_flags=("${PROJECT_ROOT}")

if [[ "${BUILD_NAME}" = "bazel" ]]; then
  driver_script="ci/kokoro/macos/build-bazel.sh"
elif [[ "${BUILD_NAME}" = "cmake-super" ]]; then
  driver_script="ci/kokoro/macos/build-cmake.sh"
  script_flags+=("super" "cmake-out/macos")
else
  echo "${COLOR_RED}$(date -u): unknown BUILD_NAME (${BUILD_NAME})."\
      "${COLOR_RESET}"
  exit 1
fi

# We need this environment variable because on macOS gRPC crashes if it cannot
# find the credentials, even if you do not use them. Some of the unit tests do
# exactly that.
echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): define GOOGLE_APPLICATION_CREDENTIALS."\
    "${COLOR_RESET}"
export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"

# Download the gRPC `roots.pem` file. On macOS gRPC does not use the native
# trust store. One needs to set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH. There was
# a PR to fix this:
#    https://github.com/grpc/grpc/pull/16246
# But it was closed without being merged, and there are open bugs:
#    https://github.com/grpc/grpc/issues/16571
echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): getting roots.pem for gRPC.${COLOR_RESET}"
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="${KOKORO_GFILE_DIR}/roots.pem"
rm -f "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
wget -O "${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}" \
    -q https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem

BRANCH="${KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH:-master}"
readonly BRANCH
echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): detected the branch name:"\
    "${BRANCH}.${COLOR_RESET}"

CACHE_BUCKET="${GOOGLE_CLOUD_CPP_KOKORO_RESULTS:-cloud-cpp-kokoro-results}"
readonly CACHE_BUCKET
CACHE_FOLDER="${CACHE_BUCKET}/build-cache/${GOOGLE_CLOUD_CPP_REPOSITORY}"
CACHE_FOLDER="${CACHE_FOLDER}/${BRANCH}"
readonly CACHE_FOLDER
CACHE_NAME="cache-macos-${BUILD_NAME}"
readonly CACHE_NAME

"${PROJECT_ROOT}/ci/kokoro/macos/download-cache.sh" \
      "${CACHE_FOLDER}" "${CACHE_NAME}" || true

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): starting build script.${COLOR_RESET}"

if "${driver_script}" "${script_flags[@]}"; then
  echo "${COLOR_GREEN}$(date -u): build script was successful.${COLOR_RESET}"
  exit_status=0
else
  echo "${COLOR_RED}$(date -u): build script reported errors.${COLOR_RESET}"
  exit_status=1
fi

if [[ "${exit_status}" -eq 0 ]]; then
  "${PROJECT_ROOT}/ci/kokoro/macos/upload-cache.sh" \
      "${CACHE_FOLDER}" "${CACHE_NAME}" || true
fi

exit ${exit_status}
