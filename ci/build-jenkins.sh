#!/usr/bin/env bash
#
# Copyright 2018 Google Inc.
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

git submodule update --init
cmake -DCMAKE_BUILD_TYPE=Release -H. -Bbuild-output
cmake --build build-output -- -j $(nproc)

readonly PROJECT_ROOT=$(pwd)
echo PROJECT_ROOT=${PROJECT_ROOT}
cd build-output
ctest --output-on-failure

readonly BTDIR="google/cloud/bigtable"

# Use the APP_PROFILE_ID if set or pass an empty string (the default profile) otherwise.
APP_PROFILE_ID="${APP_PROFILE_ID:-}"
echo APP_PROFILE_ID=${APP_PROFILE_ID}

case ${BENCHMARK} in
    endurance)
        "${BTDIR}/benchmarks/endurance_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 4 82800;
        ;;
    latency)
        "${BTDIR}/benchmarks/apply_read_latency_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}";
        ;;
    throughput)
        # The magic number 64 here is the number of threads necessary to saturate the CPU
        # on a n1-standard-4 GCE instance (that is 4 vCPUs).
        "${BTDIR}/benchmarks/apply_read_latency_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 64 1800;
        ;;
    scan)
        "${BTDIR}/benchmarks/scan_throughput_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 1 1800;
        ;;
    integration)
        (cd "${BTDIR}/tests" && "${PROJECT_ROOT}/${BTDIR}/tests/run_integration_tests_production.sh");
        (cd "${BTDIR}/examples" && "${PROJECT_ROOT}/${BTDIR}/examples/run_examples_production.sh");
        ;;
    # The following cases are used to test the script when making changes, they are not good
    # benchmarks.
    endurance-quick)
        "${BTDIR}/benchmarks/endurance_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 1 5 1000 true;
        ;;
    latency-quick)
        "${BTDIR}/benchmarks/apply_read_latency_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 1 5 1000 true;
        ;;
    throughput-quick)
        "${BTDIR}/benchmarks/apply_read_latency_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 1 5 1000 true;
        ;;
    scan-quick)
        "${BTDIR}/benchmarks/scan_throughput_benchmark" "${PROJECT_ID}" "${INSTANCE_ID}" "${APP_PROFILE_ID}" 1 5 1000 true;
        ;;
    *)
        echo "Unknown benchmark type"
        exit 1
        ;;
esac
