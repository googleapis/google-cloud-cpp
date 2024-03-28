#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Make our include guard clean against set -o nounset.
test -n "${GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__:-}" || declare -i GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__=0
if ((GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__++ != 0)); then
  return 0
fi # include guard

source module /ci/lib/io.sh

# Global variables that hold the PIDs of the Spanner emulators. These will be set
# when the emulators are started, and will be used to kill the emulators.
SPANNER_EMULATOR_PID=0
SPANNER_HTTP_EMULATOR_PID=0

# Outputs the port number that the emulator chose to listen on.
function spanner_emulator::internal::read_emulator_port() {
  local -r logfile="$1"
  shift

  local emulator_port="0"
  local -r expected=" Server address: "
  for _ in $(seq 1 8); do
    if grep -q -s "${expected}" "${logfile}"; then
      # The port number is whatever is after the last ':'.
      emulator_port=$(grep "${expected}" "${logfile}" | awk -F: '{print $NF}')
      break
    fi
    sleep 1
  done
  echo "${emulator_port}"
}

# Outputs the port number that the rest emulator chose to listen on.
function spanner_emulator::internal::read_http_emulator_port() {
  local -r logfile="$1"
  shift

  local http_emulator_port="0"
  local -r expected=": REST server listening at localhost:"
  for _ in $(seq 1 8); do
    if grep -q -s "${expected}" "${logfile}"; then
      # The port number is whatever is after the last ':'.
      http_emulator_port=$(grep "${expected}" "${logfile}" | awk -F: '{print $NF}')
      break
    fi
    sleep 1
  done
  echo "${http_emulator_port}"
}

# Starts the cloud spanner emulator. On success, exports the
# SPANNER_EMULATOR_HOST and SPANNER_EMULATOR_REST_HOST environment
# variables containing the host:port where the emulator is listening.
function spanner_emulator::start() {
  if [[ -z "${CLOUD_SDK_LOCATION:-}" ]]; then
    echo 1>&2 "You must set CLOUD_SDK_LOCATION to find the emulator"
    return 1
  fi

  # We cannot use `gcloud beta emulators spanner start` because there is no way
  # to kill the emulator at the end using that command.
  readonly SPANNER_EMULATOR_CMD="${CLOUD_SDK_LOCATION}/bin/cloud_spanner_emulator/emulator_main"
  if [[ ! -x "${SPANNER_EMULATOR_CMD}" ]]; then
    echo 1>&2 "The Cloud Spanner emulator does not seem to be installed, aborting"
    return 1
  fi
  readonly SPANNER_HTTP_EMULATOR_CMD="${CLOUD_SDK_LOCATION}/bin/cloud_spanner_emulator/gateway_main"
  if [[ ! -x "${SPANNER_HTTP_EMULATOR_CMD}" ]]; then
    echo 1>&2 "The Cloud Spanner HTTP emulator does not seem to be installed, aborting"
    return 1
  fi

  io::log "Launching Cloud Spanner emulator in the background"
  local emulator_port=0
  if [[ $# -ge 1 ]]; then
    emulator_port=$1
  fi

  rm -f emulator.log
  "${SPANNER_EMULATOR_CMD}" --host_port "localhost:${emulator_port}" >emulator.log 2>&1 </dev/null &
  SPANNER_EMULATOR_PID=$!

  emulator_port="$(spanner_emulator::internal::read_emulator_port emulator.log)"
  if [[ "${emulator_port}" = "0" ]]; then
    io::log_red "Cannot determine Cloud Spanner emulator port." >&2
    cat emulator.log >&2
    spanner_emulator::kill
    return 1
  fi

  export SPANNER_EMULATOR_HOST="localhost:${emulator_port}"
  io::log_green "Spanner emulator started at ${SPANNER_EMULATOR_HOST}"

  # Repeat the process to launch the emulator with HTTP support. We launch a
  # separate process as existing tests fail if we try and use gateway_main
  # for both gRPC and REST.
  io::log "Launching Cloud Spanner HTTP emulator in the background"
  local http_emulator_port=$((emulator_port + 1))
  if [[ $# -ge 2 ]]; then
    http_emulator_port=$2
  fi
  local grpc_emulator_port=$((http_emulator_port + 1))
  if [[ $# -ge 3 ]]; then
    grpc_emulator_port=$3
  fi

  rm -f http_emulator.log
  "${SPANNER_HTTP_EMULATOR_CMD}" --hostname "localhost" \
    --grpc_port "${grpc_emulator_port}" --http_port "${http_emulator_port}" \
    --copy_emulator_stdout=true >http_emulator.log 2>&1 </dev/null &
  SPANNER_HTTP_EMULATOR_PID=$!

  http_emulator_port="$(spanner_emulator::internal::read_http_emulator_port http_emulator.log)"
  if [[ "${http_emulator_port}" = "0" ]]; then
    io::log_red "Cannot determine Cloud Spanner HTTP emulator port." >&2
    cat http_emulator.log >&2
    spanner_emulator::kill
    return 1
  fi

  # Using https:// results in SSL errors.
  export SPANNER_EMULATOR_REST_HOST="http://localhost:${http_emulator_port}"
  io::log_green "Spanner HTTP emulator started at ${SPANNER_EMULATOR_REST_HOST}"
}

# Kills the running emulator.
function spanner_emulator::kill() {
  if (("${SPANNER_EMULATOR_PID}" > 0)); then
    echo -n "Killing Spanner Emulator [${SPANNER_EMULATOR_PID}] "
    kill "${SPANNER_EMULATOR_PID}" || echo -n "-"
    wait "${SPANNER_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
    echo -n "."
    echo " done."
    SPANNER_EMULATOR_PID=0
    echo "================ emulator.log ================"
    cat --number --show-nonprinting emulator.log
    echo "=============================================="
  fi
  if (("${SPANNER_HTTP_EMULATOR_PID}" > 0)); then
    echo -n "Killing Spanner HTTP Emulator [${SPANNER_HTTP_EMULATOR_PID}] "
    kill "${SPANNER_HTTP_EMULATOR_PID}" || echo -n "-"
    wait "${SPANNER_HTTP_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
    echo -n "."
    echo " done."
    SPANNER_HTTP_EMULATOR_PID=0
    echo "============== http_emulator.log ============="
    cat --number --show-nonprinting http_emulator.log
    echo "=============================================="
  fi
}
