#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/google/cloud/storage/tools/run_testbench_utils.sh"
source "${PROJECT_ROOT}/google/cloud/storage/examples/run_examples_utils.sh"

echo
echo "Starting Google Cloud Storage testbench."
start_testbench

# Create most likely unique names for the project and bucket so multiple tests
# can use the same testbench.
readonly PROJECT_ID="fake-project-${RANDOM}-${RANDOM}"
readonly BUCKET_NAME="fake-bucket-${RANDOM}-${RANDOM}"
readonly DESTINATION_BUCKET_NAME="destination-bucket-${RANDOM}-${RANDOM}"
readonly TOPIC_NAME="fake-topic-${RANDOM}-${RANDOM}"
readonly STORAGE_CMEK_KEY="projects/${PROJECT_ID}/locations/global/keyRings/fake-key-ring/cryptoKeys/fake-key"
readonly SERVICE_ACCOUNT="fake-service-account@example.com"
readonly HMAC_SERVICE_ACCOUNT="fake-sa-hmac@example.com"

# Most of the examples assume a bucket already exists, create one for them.
run_example ./storage_bucket_samples create-bucket-for-project \
      "${BUCKET_NAME}" "${PROJECT_ID}"
run_example ./storage_bucket_samples create-bucket-for-project \
      "${DESTINATION_BUCKET_NAME}" "${PROJECT_ID}"

run_all_storage_examples

exit_example_runner
