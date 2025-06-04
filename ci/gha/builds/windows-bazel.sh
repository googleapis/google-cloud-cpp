#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
# Define the os::* functions used in bazel.sh
source module ci/gha/builds/lib/windows.sh
source module ci/gha/builds/lib/bazel.sh

# Usage: windows-bazel.sh <compilation-mode>

test_args+=("${msvc_args[@]}")
mapfile -t args < <(bazel::common_args)
mapfile -t test_args < <(bazel::test_args)
mapfile -t msvc_args < <(bazel::msvc_args)
test_args+=("${msvc_args[@]}")
# TODO(15193): remove this workaround once the libcurl option issue is resolved.
test_args+=("--copt=-DGOOGLE_CLOUD_CPP_WINDOWS_BAZEL_CI_WORKAROUND")
if [[ $# -gt 1 ]]; then
  test_args+=("--compilation_mode=${1}")
  shift
fi
mapfile -t integration_test_args < <(bazel::integration_test_args)

if [[ -z "${VCINSTALLDIR}" ]]; then
  echo "ERROR: Missing VCINSTALLDIR, this is needed to configure Bazel+MSVC"
  exit 1
fi
export BAZEL_VC="${VCINSTALLDIR}"

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 bazel test done in %R seconds"
time {
  io::run bazelisk "${args[@]}" test "${test_args[@]}" --test_tag_filters=-integration-test -- //google/cloud:status_test "$@"
}

if [[ "${EXECUTE_INTEGRATION_TESTS}" == "true" ]]; then
  TIMEFORMAT="==> 🕑 Storage integration tests done in %R seconds"
  if [[ -n "${GHA_TEST_BUCKET:-}" ]]; then
    time {
      # gRPC requires a local roots.pem on Windows
      #   https://github.com/grpc/grpc/issues/16571
      curl -fsSL -o "${HOME}/roots.pem" https://pki.google.com/roots.pem

      io::run bazelisk "${args[@]}" test "${test_args[@]}" "${integration_test_args[@]}" \
        --test_env=GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="${HOME}/roots.pem" \
        //google/cloud/storage/tests/...
    }
  fi
fi

io::log_h1 "Starting Clean Build of include tests"
TIMEFORMAT="==> 🕑 bazel test done in %R seconds"
time {
  io::run bazelisk clean --expunge
  io::run bazelisk "${args[@]}" test "${test_args[@]}" --cache_test_results=no -- //google/cloud/storage/tests:storage_include_test-default //google/cloud/storage/tests:storage_include_test-grpc-metadata
}
