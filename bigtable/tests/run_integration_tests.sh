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

function kill_emulator {
  kill "${EMULATOR_PID}"
  wait >/dev/null 2>&1
}

echo "Launching Cloud Bigtable emulator in the background"
# The tests typically run in a Docker container, where the ports are largely
# free; when using in manual tests, you can set EMULATOR_PORT.
readonly PORT=${EMULATOR_PORT:-9000}
"${GOPATH}/bin/emulator" -port "${PORT}" >emulator.log 2>&1 </dev/null &
EMULATOR_PID=$!
if [ $? -ne 0 ]; then
  echo "Cloud Bigtable emulator failed; aborting test." >&2
  cat emulator.log >&2
  exit 1
fi

trap kill_emulator EXIT

export BIGTABLE_EMULATOR_HOST="localhost:${PORT}"
# Avoid repetition
readonly CBT_ARGS="-project emulated -instance emulated -creds default"
# Wait until the emulator starts responding.
delay=1
connected=no
readonly ATTEMPTS=$(seq 1 8)
for attempt in $ATTEMPTS; do
  if "${GOPATH}/bin/cbt" $CBT_ARGS ls >/dev/null 2>&1; then
    connected=yes
    break
  fi
  sleep $delay
  delay=$((delay * 2))
done

if [ "${connected}" = "no" ]; then
  echo "Cannot connect to Cloud Bigtable emulator; aborting test." >&2
  exit 1
else
  echo "Successfully connected to the Cloud Bigtable emulator."
fi

# Run the integration tests
echo
echo "Running Table::Apply() integration test."
# The project and instance do not matter for the Cloud Bigtable emulator.
./data_integration_test emulated data-test test-table

echo
echo "Running TableAdmin integration test."
./admin_integration_test emulated admin-test

echo
echo "Running bigtable::Filters integration tests."
./filters_integration_test emulated filters-test
