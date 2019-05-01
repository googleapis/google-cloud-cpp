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

readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/run_examples_utils.sh"
source "${BINDIR}/../tools/run_emulator_utils.sh"
source "${BINDIR}/../../../../ci/colors.sh"

# Start the emulator, setup the environment variables and traps to cleanup.
echo
echo "Running Bigtable Example programs"
start_emulators

# Setup the environment variables AS-IF running against production, but with
# fake values.
export PROJECT_ID="project-${RANDOM}-${RANDOM}"
export INSTANCE_ID="in-${RANDOM}-${RANDOM}"
export ZONE_A="fake-region1-a"
export ZONE_B="fake-region1-b"
export SERVICE_ACCOUNT="fake-sa@${PROJECT_ID}.iam.gserviceaccount.com"

"${BINDIR}/run_examples_production.sh" || EXIT_STATUS=1
"${BINDIR}/run_admin_examples_production.sh" || EXIT_STATUS=1

exit_example_runner
