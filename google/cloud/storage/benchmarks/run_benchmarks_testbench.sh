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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/google/cloud/storage/tools/run_testbench_utils.sh"
source "${PROJECT_ROOT}/ci/define-example-runner.sh"

echo
echo "Starting Google Cloud Storage testbench."
start_testbench

# Define a fake region to run the benchmarks on:
FAKE_REGION="fake-region-${RANDOM}-${RANDOM}"

# We use the same driver scripts that we use for the examples, the main
# point here is to run the benchmarks for short periods of time to smoke test
# them. We are not trying to validate the output is correct, or the performance
# has not changed.
export GOOGLE_CLOUD_PROJECT="fake-project-${RANDOM}-${RANDOM}"

run_example_usage ./storage_file_transfer_benchmark \
    --help --description
run_example ./storage_file_transfer_benchmark \
    "--project-id=${GOOGLE_CLOUD_PROJECT}" \
    "--region=${FAKE_REGION}" \
    --object-size=16KiB \
    --duration=1s

run_example ./storage_latency_benchmark \
      --duration=1 \
      --object-count=10 \
      "${FAKE_REGION}"
run_example ./storage_latency_benchmark \
      --enable-xml-api=false \
      --duration=1 \
      --object-count=10 \
      "${FAKE_REGION}"

run_example ./storage_throughput_benchmark \
      --duration=1 \
      --object-count=8 \
      --object-size=10MiB \
      "${FAKE_REGION}"
run_example ./storage_throughput_benchmark \
      --enable-xml-api=false \
      --duration=1 \
      --object-count=8 \
      --object-size=10MiB \
      "${FAKE_REGION}"

run_example_usage ./storage_throughput_vs_cpu_benchmark \
      --help --description
run_example ./storage_throughput_vs_cpu_benchmark \
      "--project-id=${GOOGLE_CLOUD_PROJECT}" \
      "--region=${FAKE_REGION}" \
      --minimum-object-size=16KiB \
      --maximum-object-size=32KiB \
      --minimum-chunk-size=16KiB \
      --maximum-chunk-size=32KiB \
      --duration=1s

if [[ "${EXIT_STATUS}" = "0" ]]; then
  TESTBENCH_DUMP_LOG=no
fi
exit "${EXIT_STATUS}"
