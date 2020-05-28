#!/usr/bin/env bash
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

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(
    cd "$(dirname "$0")/../../.."
    pwd
  )"
fi
source "${PROJECT_ROOT}/ci/kokoro/define-docker-variables.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

if [[ "${BUILD_TYPE:-}" != "Coverage" ]]; then
  # Not a code coverage build, exit silently.
  exit 0
fi

# The build script passes on the image name and the the base docker flags.
# Otherwise we would need to recompute the list here.
BUILD_IMAGE=$1
readonly BUILD_IMAGE
shift
docker_flags=("${@}")

if [[ -z "${KOKORO_GFILE_DIR:-}" ]]; then
  echo "Will not upload code coverage as KOKORO_GFILE_DIR not set."
  exit 0
fi

if [[ ! -r "${KOKORO_GFILE_DIR}/codecov-io-upload-token" ]]; then
  echo "Will not upload code coverage as the upload token is not available."
  exit 0
fi

CODECOV_TOKEN="$(cat "${KOKORO_GFILE_DIR}/codecov-io-upload-token")"
readonly CODECOV_TOKEN

# Because Kokoro checks out the code in `detached HEAD` mode there is no easy
# way to discover what is the current branch (and Kokoro does not expose the
# branch as an environment variable, like other CI systems do). We use the
# following trick:
# - Find out the current commit using git rev-parse HEAD.
# - Find out what branches contain that commit.
# - Exclude "HEAD detached" branches (they are not really branches).
# - Typically this is the single branch that was checked out by Kokoro.
VCS_BRANCH_NAME="$(git branch --no-color --contains "$(git rev-parse HEAD)" |
  grep -v 'HEAD detached' || exit 0)"
VCS_BRANCH_NAME="${VCS_BRANCH_NAME/  /}"
# When running locally, in a checked out branch, we need more cleanup.
VCS_BRANCH_NAME="${VCS_BRANCH_NAME/* /}"
readonly VCS_BRANCH_NAME

if [[ -n "${KOKORO_GITHUB_PULL_REQUEST_COMMIT:-}" ]]; then
  readonly VCS_COMMIT_ID="${KOKORO_GITHUB_PULL_REQUEST_COMMIT}"
elif [[ -n "${KOKORO_GIT_COMMIT:-}" ]]; then
  readonly VCS_COMMIT_ID="${KOKORO_GIT_COMMIT}"
else
  readonly VCS_COMMIT_ID="$(git rev-parse HEAD)"
fi

docker_flags+=(
  # The upload token for codecov.io
  "--env" "CODECOV_TOKEN=${CODECOV_TOKEN}"

  # Assert to the build
  "--env" "CI=true"

  # Let codecov.io know if this is a pull request and what is the PR number.
  "--env" "VCS_PULL_REQUEST=${KOKORO_GITHUB_PULL_REQUEST_NUMBER:-}"

  # Basic information about the commit.
  "--env" "VCS_COMMIT_ID=${VCS_COMMIT_ID}"
  "--env" "VCS_BRANCH_NAME=${VCS_BRANCH_NAME}"

  # Let codecov.io know about which build ID this is.
  "--env" "CI_BUILD_ID=${KOKORO_BUILD_NUMBER:-}"
  "--env" "CI_JOB_ID=${KOKORO_BUILD_NUMBER:-}"
)

echo -n "Uploading code coverage to codecov.io..."
# This controls the output format from bash's `time` command.
readonly TIMEFORMAT="DONE in %R seconds"
# Run the upload script from codecov.io within a Docker container. Save the log
# to a file because it can be very large (multiple MiB in size).
time {
  sudo docker run "${docker_flags[@]}" "${BUILD_IMAGE}" /bin/bash -c \
    "/bin/bash <(curl -s https://codecov.io/bash) -y /v/.codecov.yml >/v/${BUILD_OUTPUT}/codecov.log 2>&1"
  exit_status=$?
}

if [[ ${exit_status} != 0 ]]; then
  # Only print the log if there is an error.
  dump_log "${BUILD_OUTPUT}/codecov.log"
fi
