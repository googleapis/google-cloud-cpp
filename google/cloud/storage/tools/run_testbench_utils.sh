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
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

TESTBENCH_PID=0
TESTBENCH_DUMP_LOG=yes

################################################
# Terminate the Google Cloud Storage test bench
# Globals:
#   TESTBENCH_PID: the process id for the test bench
#   TESTBENCH_DUMP_LOG: if set to 'yes' the testbench log is dumped
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
  if [ "${TESTBENCH_DUMP_LOG}" = "yes" -a "testbench.log" ]; then
    echo "================ [begin testbench.log] ================"
    # Travis has a limit of ~10,000 lines, and sometimes the
    # emulator log gets too long, just print the interesting bits:
    if [ "$(wc -l "testbench.log" | awk '{print $1}')" -lt 1000 ]; then
      cat "testbench.log"
    else
      head -500 "testbench.log"
      echo "        [snip snip snip]        "
      tail -500 "testbench.log"
    fi
    echo "================ [end testbench.log] ================"
  fi
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

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  local -r testbench_port=${TESTBENCH_PORT:-8000}

  gunicorn --bind 0.0.0.0:${testbench_port} \
      --worker-class gevent \
      --access-logfile - \
      --pythonpath "${PROJECT_ROOT}/google/cloud/storage/testbench" \
      testbench:application \
      >testbench.log 2>&1 </dev/null &
  TESTBENCH_PID=$!

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
