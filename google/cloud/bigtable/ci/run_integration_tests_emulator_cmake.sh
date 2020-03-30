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

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <binary-dir> [ctest-args]"
  exit 1
fi

BINARY_DIR="$(cd "${1}"; pwd)"
readonly BINARY_DIR
shift
ctest_args=("$@")

# Configure run_emulators_utils.sh to find the instance admin emulator.
export CBT_INSTANCE_ADMIN_EMULATOR_CMD="${BINARY_DIR}/google/cloud/bigtable/tests/instance_admin_emulator"

CMDDIR="$(dirname "$0")"
readonly CMDDIR
PROJECT_ROOT="$(cd "${CMDDIR}/../../../.."; pwd)"
readonly PROJECT_ROOT
source "${PROJECT_ROOT}/google/cloud/bigtable/tools/run_emulator_utils.sh"

cd "${BINARY_DIR}"
start_emulators

NONCE="$(date +%s)-${RANDOM}"
readonly NONCE
export GOOGLE_CLOUD_PROJECT="emulated-${NONCE}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="it-${NONCE}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="fake-region1-a"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="fake-region1-b"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT="fake-sa@emulated-${NONCE}.iam.gserviceaccount.com"
export ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS="yes"
export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes

ctest -L "bigtable-integration-tests" "${ctest_args[@]}"
exit_status=$?

kill_emulators
trap '' EXIT

exit "${exit_status}"
