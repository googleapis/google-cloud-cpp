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
source "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
source "${PROJECT_ROOT}/ci/etc/quickstart-config.sh"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
log_yellow "compiling quickstart programs"
echo

readonly BAZEL_BIN="/usr/local/bin/bazel"
log_normal "Using Bazel in ${BAZEL_BIN}"

run_vars=()
bazel_args=("--test_output=errors" "--verbose_failures=true" "--keep_going")
if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=(--config "${BAZEL_CONFIG}")
fi

if [[ -r "/c/kokoro-run-key.json" ]]; then
  run_vars+=(
    "GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
    "GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
  )
fi

build_quickstart() {
  local -r library="$1"

  pushd "${PROJECT_ROOT}/google/cloud/${library}/quickstart" >/dev/null
  trap "popd >/dev/null" RETURN
  log_normal "capture bazel version"
  ${BAZEL_BIN} version
  log_normal "fetch dependencies for ${library}'s quickstart"
  "${PROJECT_ROOT}/ci/retry-command.sh" \
    "${BAZEL_BIN}" fetch -- ...

  echo
  log_yellow "Compiling ${library}'s quickstart"
  "${BAZEL_BIN}" build "${bazel_args[@]}" -- ...

  if [[ -r "/c/kokoro-run-key.json" ]]; then
    echo
    log_yellow "Running ${library}'s quickstart."
    args=()
    while IFS="" read -r line; do
      args+=("${line}")
    done < <(quickstart_arguments "${library}")
    env "${run_vars[@]}" "${BAZEL_BIN}" run "${bazel_args[@]}" \
      "--spawn_strategy=local" \
      :quickstart -- "${args[@]}"
  fi
}

errors=""
for library in $(quickstart_libraries); do
  echo
  echo "================================================================"
  log_yellow "Building ${library}'s quickstart"
  if ! build_quickstart "${library}"; then
    log_red "Building ${library}'s quickstart failed"
    errors="${errors} ${library}"
  else
    log_green "Building ${library}'s quickstart was successful"
  fi
done

echo "================================================================"
if [[ -z "${errors}" ]]; then
  log_green "All quickstart builds were successful"
else
  log_red "Build failed for ${errors}"
  exit 1
fi

exit 0
