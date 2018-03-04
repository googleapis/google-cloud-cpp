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
        bigtable/benchmarks/apply_read_latency_benchmark ${PROJECT_ID} ${INSTANCE_ID} 60 1800;
        ;;
    scan)
        bigtable/benchmarks/scan_throughput_benchmark ${PROJECT_ID} ${INSTANCE_ID} 1 1800;
        ;;
    *)
        echo "Unknown benchmark type"
        exit 1
        ;;
esac
