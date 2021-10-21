#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

# Make our include guard clean against set -o nounset.
test -n "${GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__:-}" || declare -i GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__=0
if ((GOOGLE_CLOUD_SPANNER_CI_LIB_SPANNER_EMULATOR_SH__++ != 0)); then
  return 0
fi # include guard

source module /ci/lib/io.sh

# Global variable that holds the PID of the Spanner emulator. This will be set
# when the emulator is started, and it will be used to kill the emulator.
SPANNER_EMULATOR_PID=0

# Outputs the port number that the emulator chose to listen on.
function spanner_emulator::internal::read_emulator_port() {
  local -r logfile="$1"
  shift

  local emulator_port="0"
  local -r expected=" : Server address: "
  for attempt in $(seq 1 8); do
    if grep -q "${expected}" "${logfile}"; then
      # The port number is whatever is after the last ':'.
      emulator_port=$(grep "${expected}" "${logfile}" | awk -F: '{print $NF}')
      break
    fi
    sleep 1
  done
  echo "${emulator_port}"
}

# Starts the cloud spanner emulator. On success, exports the
# SPANNER_EMULATOR_HOST environment variable containing the host:port where the
# emulator is listening.
function spanner_emulator::start() {
  io::log "Launching Cloud Spanner emulator in the background"
  if [[ -z "${CLOUD_SDK_LOCATION:-}" ]]; then
    echo 1>&2 "You must set CLOUD_SDK_LOCATION to find the emulator"
    return 1
  fi

  local emulator_port=0
  if [[ $# -ge 1 ]]; then
    emulator_port=$1
  fi

  # We cannot use `gcloud beta emulators spanner start` because there is no way
  # to kill the emulator at the end using that command.
  readonly SPANNER_EMULATOR_CMD="${CLOUD_SDK_LOCATION}/bin/cloud_spanner_emulator/emulator_main"
  if [[ ! -x "${SPANNER_EMULATOR_CMD}" ]]; then
    echo 1>&2 "The spanner emulator does not seem to be installed, aborting"
    return 1
  fi

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  "${SPANNER_EMULATOR_CMD}" --host_port "localhost:${emulator_port}" >emulator.log 2>&1 </dev/null &
  SPANNER_EMULATOR_PID=$!

  emulator_port="$(spanner_emulator::internal::read_emulator_port emulator.log)"
  if [[ "${emulator_port}" = "0" ]]; then
    echo "Cannot determine Cloud Spanner emulator port." >&2
    spanner_emulator::kill
    return 1
  fi

  export SPANNER_EMULATOR_HOST="localhost:${emulator_port}"
  io::log "Spanner emulator started at ${SPANNER_EMULATOR_HOST}"
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
  fi
}
