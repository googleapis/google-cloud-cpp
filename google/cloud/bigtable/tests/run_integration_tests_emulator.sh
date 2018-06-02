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
source ${BINDIR}/integration_tests_utils.sh
source ${BINDIR}/../tools/run_emulator_utils.sh

# Start the emulator, setup the environment variables and traps to cleanup.
start_emulators

# The project and instance do not matter for the Cloud Bigtable emulator.
# Use a unique project name to allow multiple runs of the test with
# an externally launched emulator.
readonly NONCE=$(date +%s)
run_all_integration_tests "emulated-${NONCE}"
