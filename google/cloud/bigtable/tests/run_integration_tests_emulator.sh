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

readonly BINDIR=$(dirname $0)
source ${BINDIR}/../tools/run_emulator_utils.sh

# Start the emulator, setup the environment variables and traps to cleanup.
start_emulators

# The project and instance do not matter for the Cloud Bigtable emulator.
# Use a unique project name to allow multiple runs of the test with
# an externally launched emulator.
readonly NONCE=$(date +%s)
readonly PROJECT_ID="emulated-${NONCE}"

echo
echo "Running bigtable::InstanceAdmin integration test."
./instance_admin_integration_test "${PROJECT_ID}";

echo
echo "Running bigtable::InstanceAdmin integration test."
./instance_admin_async_integration_test "${PROJECT_ID}" "us-central1-f" "us-central1-b";

echo
echo "Running bigtable::TableAdmin integration test."
./admin_integration_test "${PROJECT_ID}" "admin-test" "fake-zone-1" "fake-zone-2"

# TODO(#151) - Cleanup integration tests after emulator bugs are fixed. 
#echo
#echo "Running bigtable::TableAdmin snapshot integration test."
#./snapshot_integration_test "${PROJECT_ID}" "admin-test" "admin-test-cl1"

echo
echo "Running bigtable::Table integration test."
./data_integration_test "${PROJECT_ID}" "data-test"

echo
echo "Running bigtable::Filters integration tests."
./filters_integration_test "${PROJECT_ID}" "filters-test"

echo
echo "Running Mutation (e.g. DeleteFromColumn, SetCell) integration tests."
./mutations_integration_test "${PROJECT_ID}" "mutations-test"

echo
echo "Running TableAdmin async integration test."
./admin_async_integration_test "${PROJECT_ID}" "admin-noex-async"

echo
echo "Running TableAdmin async with futures integration test."
./admin_async_future_integration_test "${PROJECT_ID}" "admin-async-future"

echo
echo "Running Table::Async* integration test."
./data_async_integration_test "${PROJECT_ID}" "data-noex-async"

# TODO(#1338) - run snapshot_async_integration_test against emulator.
