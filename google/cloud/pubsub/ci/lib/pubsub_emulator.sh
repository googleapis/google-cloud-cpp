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
test -n "${GOOGLE_CLOUD_PUBSUB_CI_LIB_PUBSUB_EMULATOR_SH__:-}" || declare -i GOOGLE_CLOUD_PUBSUB_CI_LIB_PUBSUB_EMULATOR_SH__=0
if ((GOOGLE_CLOUD_PUBSUB_CI_LIB_PUBSUB_EMULATOR_SH__++ != 0)); then
  return 0
fi # include guard

source module /ci/lib/io.sh

# Global variable that holds the PID of the Pubsub emulator. This will be set
# when the emulator is started, and it will be used to kill the emulator.
PUBSUB_EMULATOR_PID=0

# Outputs the port number that the emulator chose to listen on.
function pubsub_emulator::internal::read_emulator_port() {
  local -r logfile="$1"
  shift

  local emulator_port="0"
  local -r expected=": Server started, listening on "
  for attempt in $(seq 1 12); do
    if grep -q "${expected}" "${logfile}"; then
      # The port number is whatever is after 'listening on'.
      emulator_port=$(grep "${expected}" "${logfile}" | awk -F' ' '{print $NF}')
      break
    fi
    sleep 1
  done
  echo "${emulator_port}"
}

# Starts the Cloud Pub/Sub emulator. On success, exports the
# PUBSUB_EMULATOR_HOST environment variable containing the host:port where the
# emulator is listening.
function pubsub_emulator::start() {
  io::log "Launching Cloud Pub/Sub emulator in the background"
  if [[ -z "${CLOUD_SDK_LOCATION:-}" ]]; then
    echo 1>&2 "You must set CLOUD_SDK_LOCATION to find the emulator"
    return 1
  fi

  # We cannot use `gcloud beta emulators pubsub start` because there is no way
  # to kill the emulator at the end using that command.
  readonly PUBSUB_EMULATOR_CMD="${CLOUD_SDK_LOCATION}/bin/gcloud"
  readonly PUBSUB_EMULATOR_ARGS=(
    "beta" "emulators" "pubsub" "start"
    "--project=${GOOGLE_CLOUD_PROJECT:-fake-project}")
  if [[ ! -x "${PUBSUB_EMULATOR_CMD}" ]]; then
    echo 1>&2 "The Cloud Pub/Sub emulator does not seem to be installed, aborting"
    cat emulator.log >&2
    return 1
  fi

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  "${PUBSUB_EMULATOR_CMD}" "${PUBSUB_EMULATOR_ARGS[@]}" >emulator.log 2>&1 </dev/null &
  PUBSUB_EMULATOR_PID=$!

  local -r emulator_port="$(pubsub_emulator::internal::read_emulator_port emulator.log)"
  if [[ "${emulator_port}" = "0" ]]; then
    echo "Cannot determine Cloud Pub/Sub emulator port." >&2
    cat emulator.log >&2
    pubsub_emulator::kill
    return 1
  fi

  export PUBSUB_EMULATOR_HOST="localhost:${emulator_port}"
  io::log "Cloud Pub/Sub emulator started at ${PUBSUB_EMULATOR_HOST}"
}

# Kills the running emulator.
function pubsub_emulator::kill() {
  if (("${PUBSUB_EMULATOR_PID}" > 0)); then
    echo -n "Killing Pub/Sub Emulator [${PUBSUB_EMULATOR_PID}] "
    kill "${PUBSUB_EMULATOR_PID}" || echo -n "-"
    wait "${PUBSUB_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
    echo -n "."
    echo " done."
    PUBSUB_EMULATOR_PID=0
  fi
}
