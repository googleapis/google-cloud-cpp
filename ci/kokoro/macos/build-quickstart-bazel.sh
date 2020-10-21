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
source module etc/integration-tests-config.sh
source module etc/quickstart-config.sh
source module lib/io.sh

echo
echo "================================================================"
io::log_yellow "update or install Bazel."

# macOS does not have sha256sum by default, but `shasum -a 256` does the same
# thing:
function sha256sum() { shasum -a 256 "$@"; }
export -f sha256sum

mkdir -p "cmake-out/download"
(
  cd "cmake-out/download"
  "${PROJECT_ROOT}/ci/install-bazel.sh" >/dev/null
)

echo
echo "================================================================"
readonly BAZEL_BIN="$HOME/bin/bazel"
io::log "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version
"${BAZEL_BIN}" shutdown

bazel_args=(
  # On macOS gRPC does not compile correctly unless one defines this:
  "--copt=-DGRPC_BAZEL_BUILD"
  # We need this environment variable because on macOS gRPC crashes if it
  # cannot find the credentials, even if you do not use them. Some of the
  # unit tests do exactly that.
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going")

run_quickstart="false"
readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly CREDENTIALS_FILE="${CONFIG_DIR}/kokoro-run-key.json"
readonly ROOTS_PEM_SOURCE="https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem"
readonly BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

if [[ -r "${CREDENTIALS_FILE}" ]]; then
  io::log "Using bazel remote cache: ${BAZEL_CACHE}/macos/${BUILD_NAME:-}"
  bazel_args+=("--remote_cache=${BAZEL_CACHE}/macos/${BUILD_NAME:-}")
  bazel_args+=("--google_credentials=${CREDENTIALS_FILE}")
  # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
  # and https://github.com/bazelbuild/bazel/issues/3360
  bazel_args+=("--experimental_guard_against_concurrent_changes")
  if [[ -r "${CONFIG_DIR}/roots.pem" ]]; then
    run_quickstart="true"
  elif wget -O "${CONFIG_DIR}/roots.pem" -q "${ROOTS_PEM_SOURCE}"; then
    run_quickstart="true"
  fi
fi
readonly run_quickstart

echo "================================================================"
cd "${PROJECT_ROOT}"

build_quickstart() {
  local -r library="$1"

  pushd "${PROJECT_ROOT}/google/cloud/${library}/quickstart" >/dev/null
  trap "popd >/dev/null" RETURN
  io::log "capture bazel version"
  ${BAZEL_BIN} version
  for repeat in 1 2 3; do
    echo
    io::log_yellow "Fetching deps for ${library}'s quickstart [${repeat}/3]."
    if "${BAZEL_BIN}" fetch ...; then
      break
    else
      io::log_yellow "bazel fetch failed with $?"
    fi
  done

  echo
  io::log_yellow "Compiling ${library}'s quickstart"
  "${BAZEL_BIN}" build "${bazel_args[@]}" ...

  if [[ "${run_quickstart}" == "true" ]]; then
    echo
    io::log_yellow "Running ${library}'s quickstart."
    args=()
    while IFS="" read -r line; do
      args+=("${line}")
    done < <(quickstart::arguments "${library}")
    env "GOOGLE_APPLICATION_CREDENTIALS=${CREDENTIALS_FILE}" \
      "GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${CONFIG_DIR}/roots.pem" \
      "${BAZEL_BIN}" run "${bazel_args[@]}" "--spawn_strategy=local" \
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

echo "================================================================"
if [[ -z "${errors}" ]]; then
  io::log_green "All quickstart builds were successful"
else
  io::log_red "Build failed for ${errors}"
  exit 1
fi

exit 0
