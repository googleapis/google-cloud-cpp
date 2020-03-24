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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.." || exit; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

TESTBENCH_PID=0

################################################
# Terminate the Google Cloud Storage test bench
# Globals:
#   TESTBENCH_PID: the process id for the test bench
#   COLOR_*: colorize output messages, defined in colors.sh
# Arguments:
#   None
# Returns:
#   None
################################################
kill_testbench() {
  echo "${COLOR_GREEN}[ -------- ]${COLOR_RESET} Integration test environment tear-down."
  echo -n "Killing testbench server [${TESTBENCH_PID}] ... "
  kill "${TESTBENCH_PID}"
  wait "${TESTBENCH_PID}" >/dev/null 2>&1
  echo "done."
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Integration test environment tear-down."

}

################################################
# Start the Google Cloud Storage test bench
# Globals:
#   TESTBENCH_PORT: the listening port for the test bench, 8000 if not set.
#   HTTPBIN_ENDPOINT: the httpbin endpoint on the test bench.
#   TESTBENCH_PID: the process id for the test bench.
#   CLOUD_STORAGE_TESTBENCH_ENDPOINT: the google cloud storage endpoint for the
#       test bench.
#   COLOR_*: colorize output messages, defined in colors.sh
# Arguments:
#   None
# Returns:
#   None
################################################
start_testbench() {
  echo "${COLOR_GREEN}[ -------- ]${COLOR_RESET} Integration test environment set-up"
  echo "Launching testbench emulator in the background"
  trap kill_testbench EXIT

  gunicorn --bind 0.0.0.0:0 \
      --worker-class gevent \
      --access-logfile - \
      --pythonpath "${PROJECT_ROOT}/google/cloud/storage/testbench" \
      testbench:application \
      >testbench.log 2>&1 </dev/null &
  TESTBENCH_PID=$!

  local testbench_port=""
  local -r listening_at='Listening at: http://0.0.0.0:\([1-9][0-9]*\)'
  for attempt in $(seq 1 8); do
    if [[ -r testbench.log ]]; then
        testbench_port=$(sed -n "s,^.*${listening_at}.*$,\1,p" testbench.log)
        [[ -n "${testbench_port}" ]] && break
    fi
    sleep 1
  done

  if [[ -z "${testbench_port}" ]]; then
    echo "Cannot find listening port for testbench." >&2
    exit 1
  fi

  export HTTPBIN_ENDPOINT="http://localhost:${testbench_port}/httpbin"
  export CLOUD_STORAGE_TESTBENCH_ENDPOINT="http://localhost:${testbench_port}"

  delay=1
  connected=no
  for attempt in $(seq 1 8); do
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
    echo "Successfully connected to testbench [${TESTBENCH_PID}]"
  fi

  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET} Integration test environment set-up."
}
