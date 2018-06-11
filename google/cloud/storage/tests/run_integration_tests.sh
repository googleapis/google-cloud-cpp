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

readonly BINDIR="$(dirname $0)"
source "${BINDIR}/../../../../ci/colors.sh"

TESTBENCH_PID=0

function kill_testbench {
  echo "${COLOR_GREEN}[ -------- ]${COLOR_RESET} Integration test environment tear-down."
  echo -n "Killing testbench server [${TESTBENCH_PID}] ... "
  kill "${TESTBENCH_PID}"
  wait >/dev/null 2>&1
  echo "done."
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Integration test environment tear-down."
}

function start_testbench {
  echo "${COLOR_GREEN}[ -------- ]${COLOR_RESET} Integration test environment set-up"
  echo "Launching testbench emulator in the background"
  trap kill_testbench EXIT

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  readonly PORT=${TESTBENCH_PORT:-8000}

  "${BINDIR}/testbench.py" --port ${PORT} >testbench.log 2>&1 </dev/null &
  TESTBENCH_PID=$!

  export HTTPBIN_ENDPOINT="http://localhost:${PORT}/httpbin"
  export CLOUD_STORAGE_TESTBENCH_ENDPOINT="http://localhost:${PORT}"

  delay=1
  connected=no
  readonly ATTEMPTS=$(seq 1 8)
  for attempt in $ATTEMPTS; do
    if curl "${HTTPBIN_ENDPOINT}/get" >/dev/null 2>&1; then
      connected=yes
      break
    fi
    sleep $delay
    delay=$((delay * 2))
  done

  if [ "${connected}" = "no" ]; then
    echo "Cannot connect to testbench; aborting test." >&2
    exit 1
  else
    echo "Successfully connected to testbench"
  fi

  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Integration test environment set-up."
}

# Create most likely unique names for the project and bucket so multiple tests
# can use the same testbench.
readonly PROJECT_ID="fake-project-$(date +%s)"
readonly BUCKET_NAME="fake-bucket-$(date +%s)"

echo
echo "Running Storage integration tests against local servers."
start_testbench

echo
echo "Running storage::internal::CurlRequest integration test."
./curl_request_integration_test

echo
echo "Running storage::Bucket integration tests."
./bucket_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

exit 0
