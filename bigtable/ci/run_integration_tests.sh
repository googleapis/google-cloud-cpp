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

# This script should is called from the build directory, and it finds other
# scripts in the source directory using its own path.
readonly BINDIR="$(dirname $0)"
source "${BINDIR}/../../ci/colors.sh"

(cd bigtable/tests && "${BINDIR}/../tests/run_integration_tests_emulator.sh")
(cd bigtable/tests && "${BINDIR}/../examples/run_examples_emulator.sh")

# To improve coverage (and avoid bitrot), run the Bigtable benchmarks using the
# embedded server.  The performance in the Travis and AppVeyor CI builds is not
# consistent enough to use the results, but we want to detect crashes and ensure
# the code at least is able to run as soon as possible.
for benchmark in endurance apply_read_latency scan_throughput; do
  log="$(mktemp --tmpdir "bigtable_benchmarks_${benchmark}.XXXXXXXXXX.log")"
  if [ ! -x ./bigtable/benchmarks/${benchmark}_benchmark ]; then
    echo "${COLOR_YELLOW}[ SKIPPED  ]${COLOR_RESET} ${benchmark} benchmark"
    continue
  fi
  echo "${COLOR_GREEN}[ RUN      ]${COLOR_RESET} ${benchmark}"
  "./bigtable/benchmarks/${benchmark}_benchmark" unused unused 1 5 1000 true >"${log}" 2>&1 </dev/null
  if [ $? = 0 ]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET} ${benchmark} benchmark"
  else
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET} ${benchmark} benchmark"
    echo
    echo "================ ${LOG} ================"
    cat "${log}"
    echo "================ ${LOG} ================"
  fi
done
