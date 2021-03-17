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

source "$(dirname "$0")/../../lib/init.sh"
source module /ci/etc/integration-tests-config.sh
source module /ci/etc/quickstart-config.sh
source module /ci/lib/io.sh

if [[ $# != 2 ]]; then
  # The arguments are ignored, but required for compatibility with
  # build-in-docker-cmake.sh
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
io::log_yellow "compiling quickstart programs"
echo

readonly BAZEL_BIN="/usr/local/bin/bazel"
io::log "Using Bazel in ${BAZEL_BIN}"

run_vars=()
bazel_args=(
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going"
  "--experimental_convenience_symlinks=ignore"
)
if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=(--config "${BAZEL_CONFIG}")
fi

readonly BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

if [[ -r "/c/kokoro-run-key.json" ]]; then
  run_vars+=(
    "GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
    "GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
  )
  io::log "Using bazel remote cache: ${BAZEL_CACHE}/docker/${BUILD_NAME}"
  bazel_args+=("--remote_cache=${BAZEL_CACHE}/docker/${BUILD_NAME}")
  bazel_args+=("--google_credentials=/c/kokoro-run-key.json")
  # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
  # and https://github.com/bazelbuild/bazel/issues/3360
  bazel_args+=("--experimental_guard_against_concurrent_changes")
fi

build_quickstart() {
  local -r library="$1"

  # Additional dependencies, these are not downloaded by `bazel fetch ...`,
  # but are needed to compile the code
  external=(
    @local_config_platform//...
    @local_config_cc_toolchains//...
    @local_config_sh//...
    @go_sdk//...
    @remotejdk11_linux//:jdk
  )

  pushd "${PROJECT_ROOT}/google/cloud/${library}/quickstart" >/dev/null
  trap "popd >/dev/null" RETURN
  io::log "capture bazel version"
  ${BAZEL_BIN} version
  io::log "fetch dependencies for ${library}'s quickstart"
  # retry up to 3 times with exponential backoff, initial interval 120s
  "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
    "${BAZEL_BIN}" fetch ... "${external[@]}"

  echo
  io::log_yellow "Compiling ${library}'s quickstart"
  "${BAZEL_BIN}" build "${bazel_args[@]}" ...

  if [[ -r "/c/kokoro-run-key.json" ]]; then
    echo
    io::log_yellow "Running ${library}'s quickstart."
    args=()
    while IFS="" read -r line; do
      args+=("${line}")
    done < <(quickstart::arguments "${library}")
    env "${run_vars[@]}" "${BAZEL_BIN}" run "${bazel_args[@]}" \
      "--spawn_strategy=local" \
      :quickstart -- "${args[@]}"
  fi
}

errors=""
for library in $(quickstart::libraries); do
  echo
  echo "================================================================"
  io::log_yellow "Building ${library}'s quickstart"
  if ! build_quickstart "${library}"; then
    io::log_red "Building ${library}'s quickstart failed"
    errors="${errors} ${library}"
  else
    io::log_green "Building ${library}'s quickstart was successful"
  fi
done

echo
echo "================================================================"
io::log_yellow "Verifying deprecated (but not retired) targets"
env -C ci/verify_deprecated_targets \
  "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
  "${BAZEL_BIN}" fetch ... "${external[@]}"
if ! env -C ci/verify_deprecated_targets "${BAZEL_BIN}" test ... "${bazel_args[@]}"; then
  io::log_red "Building deprecated targets failed"
  errors="${errors} 'deprecated targets'"
fi

echo
echo "================================================================"
io::log_yellow "Verifying current targets"
env -C ci/verify_current_targets \
  "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
  "${BAZEL_BIN}" fetch ... "${external[@]}"
if ! env -C ci/verify_current_targets "${BAZEL_BIN}" test ... "${bazel_args[@]}"; then
  io::log_red "Building current targets failed"
  errors="${errors} 'current targets'"
fi

echo "================================================================"
if [[ -z "${errors}" ]]; then
  io::log_green "All quickstart builds were successful"
else
  io::log_red "Build failed for ${errors}"
  exit 1
fi

exit 0
