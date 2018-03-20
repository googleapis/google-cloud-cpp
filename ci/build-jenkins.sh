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

git submodule update --init --recursive
cmake -DCMAKE_BUILD_TYPE=Release -H. -Bbuild-output
cmake --build build-output -- -j $(nproc)

cd build-output
ctest --output-on-failure

case ${BENCHMARK} in
    endurance)
        bigtable/benchmarks/endurance_benchmark ${PROJECT_ID} ${INSTANCE_ID} 4 82800;
        ;;
    latency)
        bigtable/benchmarks/apply_read_latency_benchmark ${PROJECT_ID} ${INSTANCE_ID};
        ;;
    throughput)
        # The magic number 64 here is the number of threads necessary to saturate the CPU
        # on a n1-standard-4 GCE instance (that is 4 vCPUs).
        bigtable/benchmarks/apply_read_latency_benchmark ${PROJECT_ID} ${INSTANCE_ID} 64 1800;
        ;;
    scan)
        bigtable/benchmarks/scan_throughput_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 1800;
        ;;
    integration)
        (cd bigtable/tests && ../../../bigtable/tests/run_integration_tests_production.sh);
        ;;
    # The following cases are used to test the script when making changes, they are not good
    # benchmarks.
    endurance-quick)
        bigtable/benchmarks/endurance_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 5 1000 true;
        ;;
    latency-quick)
        bigtable/benchmarks/apply_read_latency_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 5 1000 true;
        ;;
    throughput-quick)
        bigtable/benchmarks/apply_read_latency_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 5 1000 true;
        ;;
    scan-quick)
        bigtable/benchmarks/scan_throughput_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 5 1000 true;
        ;;
    *)
        echo "Unknown benchmark type"
        exit 1
        ;;
esac
