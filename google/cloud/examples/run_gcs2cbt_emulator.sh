#!/usr/bin/env bash
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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../.."; pwd)"
fi
readonly CBT_INSTANCE_ADMIN_EMULATOR_CMD="../bigtable/tests/instance_admin_emulator"
source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/ci/define-example-runner.sh"
source "${PROJECT_ROOT}/google/cloud/storage/tools/run_testbench_utils.sh"
source "${PROJECT_ROOT}/google/cloud/bigtable/tools/run_emulator_utils.sh"

kill_emulator_and_testbench() {
  kill_emulators || /bin/true
  kill_testbench || /bin/true
}

echo "${COLOR_GREEN}[ -------- ]${COLOR_RESET} gcs2cbt test environment"
start_testbench
start_emulators

# Reset the trap to kill both the GCS testbench and storage emulator.
trap kill_emulator_and_testbench EXIT

readonly PROJECT_ID="fake-project-${RANDOM}-${RANDOM}"
readonly INSTANCE_ID="in-${RANDOM}-${RANDOM}"
readonly ZONE_ID="fake-zone"
readonly BUCKET_NAME="fake-bucket-${RANDOM}-${RANDOM}"
readonly TABLE="csv-table-${RANDOM}"

run_example ../bigtable/examples/table_admin_snippets \
    create-table "${PROJECT_ID}" "${INSTANCE_ID}" "${TABLE}"

run_example ../storage/examples/storage_bucket_samples \
      create-bucket-for-project \
      "${BUCKET_NAME}" "${PROJECT_ID}"

readonly CSV_FILE="$(mktemp -t "contents.XXXXXX")"
cat >"${CSV_FILE}" <<-_EOF_
RowId,Header1,Header2,Header3
1,v1,v2,v3
3,v1,v2,v3
_EOF_

object_name="contents.csv"
run_example ../storage/examples/storage_object_samples \
      upload-file "${CSV_FILE}" "${BUCKET_NAME}" "${object_name}"

run_example ./gcs2cbt --key=1 --separator=, \
    "${PROJECT_ID}" "${INSTANCE_ID}" "${TABLE}"  "fam" \
    "${BUCKET_NAME}" "${object_name}"

run_example_usage ./gcs2cbt --help

TESTBENCH_DUMP_LOG=no
