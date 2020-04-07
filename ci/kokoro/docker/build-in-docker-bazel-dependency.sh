#!/usr/bin/env bash
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

if [[ $# != 2 ]]; then
  # The arguments are ignored, but required for compatibility with
  # build-in-docker-cmake.sh
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# This script is supposed to run inside a Docker container, see
# ci/kokoro/build.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="/v"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
log_yellow "compiling quickstart programs"
echo

readonly BAZEL_BIN="/usr/local/bin/bazel"
echo "$(date -u): ising Bazel in ${BAZEL_BIN}"

run_vars=()
bazel_args=("--test_output=errors" "--verbose_failures=true" "--keep_going")
if [[ -n "${BAZEL_CONFIG}" ]]; then
    bazel_args+=(--config "${BAZEL_CONFIG}")
fi

declare -A quickstart_args=()

if [[ -r "/c/test-configuration.sh" ]]; then
  # shellcheck disable=SC1091
  source "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
  # TODO(#3604) - figure out how to run pass arguments safely
  quickstart_args=(
    ["storage"]="${GOOGLE_CLOUD_PROJECT} ${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    ["bigtable"]="${GOOGLE_CLOUD_PROJECT} ${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID} quickstart"
  )
  run_vars+=(
      "GOOGLE_APPLICATION_CREDENTIALS=/c/service-account.json"
      "GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
  )
fi

build_service() {
  local -r service="$1"

  echo "================================================================"
  log_yellow "building ${service}"
  ( cd "google/cloud/${service}/quickstart";
    echo "$(date -u): capture bazel version"
    ${BAZEL_BIN} version
    echo "$(date -u): fetch dependencies to avoid flaky builds"
    "${PROJECT_ROOT}/ci/retry-command.sh" \
        "${BAZEL_BIN}" fetch -- ...
    echo
    echo "$(date -u): compiling quickstart program for ${service}"
    "${BAZEL_BIN}" build  "${bazel_args[@]}" -- ...

    if [[ -r "/c/test-configuration.sh" ]]; then
      echo "$(date -u): running quickstart program for ${service}"
      env "${run_vars[@]}" "${BAZEL_BIN}" run "${bazel_args[@]}" \
          "--spawn_strategy=local" \
          :quickstart -- "${quickstart_args["${service}"]}"
    fi
  )
}

errors=""
for service in bigtable storage; do
  if ! build_service "${service}"; then
    errors="${errors} ${service}"
  fi
done

echo "================================================================"
if [[ -z "${errors}" ]]; then
  log_green "Build finished"
else
  log_red "Build failed for ${errors}"
fi

exit 0
