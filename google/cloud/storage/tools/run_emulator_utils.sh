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

source module /ci/lib/io.sh

EMULATOR_PID=0

################################################
# Terminate the Google Cloud Storage test bench
# Globals:
#   EMULATOR_PID: the process id for the test bench
#   IO_COLOR_*: colorize output messages, defined in ci/lib/io.sh
# Arguments:
#   None
# Returns:
#   None
################################################
kill_emulator() {
  echo "${IO_COLOR_GREEN}[ -------- ]${IO_RESET} Integration test environment tear-down."
  echo -n "Killing emulator server [${EMULATOR_PID}] ... "
  kill "${EMULATOR_PID}"
  wait "${EMULATOR_PID}" >/dev/null 2>&1
  echo "done."
  echo "${IO_COLOR_GREEN}[ ======== ]${IO_RESET} Integration test environment tear-down."

}

################################################
# Start the Google Cloud Storage test bench
# Globals:
#   EMULATOR_PORT: the listening port for the test bench, 8000 if not set.
#   HTTPBIN_ENDPOINT: the httpbin endpoint on the test bench.
#   EMULATOR_PID: the process id for the test bench.
#   CLOUD_STORAGE_EMULATOR_ENDPOINT: the google cloud storage endpoint for the
#       test bench.
#   IO_COLOR_*: colorize output messages, defined in lib/io.sh
# Arguments:
#   None
# Returns:
#   None
################################################
start_emulator() {
  local port="${1:-0}"

  echo "${IO_COLOR_GREEN}[ -------- ]${IO_RESET} Integration test environment set-up"
  echo "Launching Cloud Storage emulator in the background"
  trap kill_emulator EXIT

  gunicorn --bind "0.0.0.0:${port}" \
    --worker-class sync \
    --threads "$(nproc)" \
    --access-logfile - \
    --chdir "${PROJECT_ROOT}/google/cloud/storage/emulator" \
    "emulator:run()" \
    >gcs_emulator.log 2>&1 </dev/null &
  EMULATOR_PID=$!

  local emulator_port=""
  local -r listening_at='Listening at: http://0.0.0.0:\([1-9][0-9]*\)'
  for attempt in $(seq 1 8); do
    if [[ -r gcs_emulator.log ]]; then
      emulator_port=$(sed -n "s,^.*${listening_at}.*$,\1,p" gcs_emulator.log)
      [[ -n "${emulator_port}" ]] && break
    fi
    sleep 1
  done

  if [[ -z "${emulator_port}" ]]; then
    echo "${IO_COLOR_RED}Cannot find listening port for emulator.${IO_RESET}" >&2
    cat gcs_emulator.log
    exit 1
  fi

  export HTTPBIN_ENDPOINT="http://localhost:${emulator_port}/httpbin"
  export CLOUD_STORAGE_EMULATOR_ENDPOINT="http://localhost:${emulator_port}"

  delay=1
  connected=no
  for _ in $(seq 1 8); do
    if curl "${HTTPBIN_ENDPOINT}/get" >/dev/null 2>&1; then
      connected=yes
      break
    fi
    sleep $delay
    delay=$((delay * 2))
  done

  if [[ "${connected}" = "no" ]]; then
    echo "${IO_COLOR_RED}Cannot connect to emulator; aborting test.${IO_RESET}" >&2
    cat gcs_emulator.log
    exit 1
  else
    echo "Successfully connected to emulator [${EMULATOR_PID}]"
  fi

  port="${2:-0}"
  local grpc_port=""
  grpc_port=$(curl -s --retry 5 --retry-max-time 40 "http://localhost:${emulator_port}/start_grpc?port=${port}")

  if [ "${grpc_port}" -eq "${grpc_port}" ] 2>/dev/null; then
    echo "Successfully connected to gRPC server at port ${grpc_port}"
  else
    echo "${IO_COLOR_RED}${grpc_port} must be an integer" >&2
    echo "${IO_COLOR_RED}Cannot connect to gRPC server; aborting test.${IO_RESET}" >&2
    cat gcs_emulator.log
    exit 1
  fi
  export CLOUD_STORAGE_GRPC_ENDPOINT="localhost:${grpc_port}"

  echo "${IO_COLOR_GREEN}[ ======== ]${IO_RESET} Integration test environment set-up."
}
