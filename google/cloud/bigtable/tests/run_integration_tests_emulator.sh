#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

readonly BINDIR=$(dirname "$0")
source "${BINDIR}/../tools/run_emulator_utils.sh"

# Start the emulator, setup the environment variables and traps to cleanup.
start_emulators

# The project and instance do not matter for the Cloud Bigtable emulator.
# Use a unique project name to allow multiple runs of the test with
# an externally launched emulator.
readonly NONCE="${RANDOM}-${RANDOM}"
export PROJECT_ID="emulated-${NONCE}"
export INSTANCE_ID="it-${NONCE}"
export ZONE_A="fake-region1-a"
export ZONE_B="fake-region1-b"
export SERVICE_ACCOUNT="fake-sa@${PROJECT_ID}.iam.gserviceaccount.com"

# We run the same tests we run in production, just with different settings.
"${BINDIR}/run_integration_tests_production.sh"
"${BINDIR}/run_admin_integration_tests_production.sh"
