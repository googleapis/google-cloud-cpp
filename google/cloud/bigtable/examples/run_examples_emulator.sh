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

readonly BINDIR="$(dirname $0)"
source "${BINDIR}/run_examples_utils.sh"
source "${BINDIR}/../tools/run_emulator_utils.sh"
source "${BINDIR}/../../../../ci/colors.sh"

# Start the emulator, setup the environment variables and traps to cleanup.
echo
echo "Running Bigtable Example programs"
start_emulators

# Use a (likely unique) project id for the emulator.
readonly PROJECT_ID="project-$(date +%s)"
readonly ZONE_ID="fake-zone"

# The examples are noisy,
log="$(mktemp -t "bigtable_examples.XXXXXX")"
for example in instance_admin table_admin data; do
  echo "${COLOR_GREEN}[ RUN      ]${COLOR_RESET} ${example}"
  run_all_${example}_examples "${PROJECT_ID}" "${ZONE_ID}" >${log} 2>&1 </dev/null
  if [ $? = 0 ]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET} ${example} examples"
  else
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET} ${example} examples"
    echo
    echo "================ ${log} ================"
    cat ${log}
    echo "================ ${log} ================"
  fi
  /bin/rm "${log}"
done
