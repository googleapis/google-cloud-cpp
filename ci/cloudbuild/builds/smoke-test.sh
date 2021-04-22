#!/bin/bash
#
# Copyright 2021 Google LLC
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
source module ci/cloudbuild/builds/lib/cmake.sh

# Run the checkers and the clang-tidy builds
source module ci/cloudbuild/builds/checkers.sh
source module ci/cloudbuild/builds/clang-tidy.sh

# Run the integration tests that only use the emulators
readonly EMULATOR_SCRIPT="run_integration_tests_emulator_cmake.sh"
if [[ -z "${NCPU:-}" ]]; then
  NCPU="$(nproc)"
fi

ctest_args=("--output-on-failure" "-j" "${NCPU}" --timeout 300)

io::log_h2 "running pubsub integration tests (with emulator)"
"${PROJECT_ROOT}/google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
  "cmake-out" "${ctest_args[@]}" -L integration-test-emulator

# TODO(#441) - remove the retry-command wrapping.
# Sometimes the integration tests manage to crash the Bigtable emulator.
# Manually restarting the build clears up the problem, but that is just a
# waste of everybody's time. Use a (short) timeout to run the test and try
# 3 times.
io::log_h2 "running bigtable integration tests (with emulator)"
env CBT=/usr/local/google-cloud-sdk/bin/cbt \
  CBT_EMULATOR=/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator \
  GOPATH="${GOPATH:-}" \
  ./ci/retry-command.sh 3 0 \
  "./google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
  "cmake-out" "${ctest_args[@]}" -L integration-test-emulator

io::log_h2 "running storage integration tests (with emulator)"
"${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
  "cmake-out" "${ctest_args[@]}" -L integration-test-emulator

io::log_h2 "running spanner integration tests (with emulator)"
"${PROJECT_ROOT}/google/cloud/spanner/ci/${EMULATOR_SCRIPT}" \
  "cmake-out" "${ctest_args[@]}" -L integration-test-emulator

io::log_h2 "running generator integration tests via CTest"
"${PROJECT_ROOT}/generator/ci/${EMULATOR_SCRIPT}" \
  "cmake-out" "${ctest_args[@]}"
