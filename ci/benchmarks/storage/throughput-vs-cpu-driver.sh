#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

echo "# ================================================================"

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <logging-bucket> [benchmark args...]"
  exit 1
fi

readonly LOGGING_BUCKET="$1"
shift 1

# Disable -e because we always want to upload the log, even partial results
# are useful.
set +e
/r/storage_throughput_vs_cpu_benchmark "$@" 2>&1 | tee tp-vs-cpu.txt
exit_status=$?

# We need to upload the results to a unique object in GCS. The program creates
# a unique bucket to run, so we can use the same bucket name as part of a unique
# name.
readonly B="$(sed -n 's/# Running test on bucket: \(.*\)/\1/p' tp-vs-cpu.txt)"
# While the bucket name (which is randomly generated and fairly long) is almost
# guaranteed to be a unique name, we also want to easily search for "all the
# uploads around this time". Using a timestamp as a prefix makes that relatively
# easy.
readonly D="$(date -u +%Y-%m-%dT%H:%M:%S)"
readonly OBJECT_NAME="throughput-vs-cpu/${D}-${B}.txt"

/r/storage_object_samples upload-file tp-vs-cpu.txt \
    "${LOGGING_BUCKET}" "${OBJECT_NAME}" >/dev/null

exit ${exit_status}
