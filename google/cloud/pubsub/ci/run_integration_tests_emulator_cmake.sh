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

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/etc/integration-tests-config.sh
source module /google/cloud/pubsub/ci/lib/pubsub_emulator.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <binary-dir> [ctest-args]"
  exit 1
fi

BINARY_DIR="$(
  cd "${1}"
  pwd
)"
readonly BINARY_DIR
shift
ctest_args=("$@")

# Use the same configuration parameters as we use for testing against
# production. Easier to maintain just one copy.
export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes
export GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,100,WARNING"
export GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc"
export GOOGLE_CLOUD_CPP_TRACING_OPTIONS="truncate_string_field_longer_than=512"

cd "${BINARY_DIR}"
# Start the emulator and arranges to kill it
pubsub_emulator::start
trap pubsub_emulator::kill EXIT

ctest -R "^pubsub_" "${ctest_args[@]}"
