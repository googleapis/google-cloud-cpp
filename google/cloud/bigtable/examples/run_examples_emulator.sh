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
readonly INSTANCE_ID="in-$(date +%s)-${RANDOM}"
readonly ZONE_ID="fake-zone"

run_all_data_examples "${PROJECT_ID}" "${INSTANCE_ID}"
run_quickstart_example "${PROJECT_ID}" "${INSTANCE_ID}"
run_hello_world_example "${PROJECT_ID}" "${INSTANCE_ID}"
run_hello_app_profile_example "${PROJECT_ID}" "${INSTANCE_ID}"
run_all_table_admin_examples "${PROJECT_ID}" "${ZONE_ID}"
run_all_instance_admin_examples "${PROJECT_ID}" "${ZONE_ID}"

exit ${EXIT_STATUS}
