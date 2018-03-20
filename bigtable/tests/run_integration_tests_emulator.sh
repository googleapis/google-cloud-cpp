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
  cat emulator.log >&2
}

readonly BINDIR=$(dirname $0)
source ${BINDIR}/integration_tests_utils.sh

echo "Launching Cloud Bigtable emulator in the background"
# The tests typically run in a Docker container, where the ports are largely
# free; when using in manual tests, you can set EMULATOR_PORT.
readonly PORT=${EMULATOR_PORT:-9000}
"${CBT_EMULATOR_CMD}" -port "${PORT}" >emulator.log 2>&1 </dev/null &
EMULATOR_PID=$!

trap kill_emulator EXIT

export BIGTABLE_EMULATOR_HOST="localhost:${PORT}"
# Avoid repetition
readonly CBT_ARGS="-project emulated -instance emulated -creds default"
# Wait until the emulator starts responding.
delay=1
connected=no
readonly ATTEMPTS=$(seq 1 8)
for attempt in $ATTEMPTS; do
  if "${CBT_CMD}" $CBT_ARGS ls >/dev/null 2>&1; then
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

# The project and instance do not matter for the Cloud Bigtable emulator.
# Use a unique project name to allow multiple runs of the test with
# an externally launched emulator.
readonly NONCE=$(date +%s)
run_all_integration_tests "emulated-${NONCE}"
