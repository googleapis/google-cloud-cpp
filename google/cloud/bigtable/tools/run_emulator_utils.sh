#!/usr/bin/env bash
# Copyright 2018 Google LLC
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

EMULATOR_PID=0
INSTANCE_ADMIN_EMULATOR_PID=0

readonly CBT_CMD="${CBT:-${GOPATH}/bin/cbt}"
readonly CBT_EMULATOR_CMD="${CBT_EMULATOR:-${GOPATH}/bin/emulator}"
if [ -z "${CBT_INSTANCE_ADMIN_EMULATOR_CMD+x}" ]; then
  readonly CBT_INSTANCE_ADMIN_EMULATOR_CMD="../tests/instance_admin_emulator"
fi

function kill_emulators() {
  echo -n "Killing Bigtable Emulators [${EMULATOR_PID} ${INSTANCE_ADMIN_EMULATOR_PID}] "
  kill "${EMULATOR_PID}" || echo -n "-"
  echo -n "."
  wait "${EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo -n "."
  kill "${INSTANCE_ADMIN_EMULATOR_PID}" || echo -n "-"
  echo -n "."
  wait "${INSTANCE_ADMIN_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo -n "."
  echo " done."
}

################################################
# Wait until the port number is printed in the log file for one of the
# emulators
# Globals:
#   None
# Arguments:
#   logfile: the name of the logfile to wait on.
# Returns:
#   None
################################################
wait_for_port() {
  local -r logfile="$1"
  shift

  local emulator_port="0"
  local -r expected='Cloud Bigtable emulator running on'
  for attempt in $(seq 1 8); do
    if grep -q "${expected}" "${logfile}"; then
      # The port number is whatever is after the last ':'. Note that on IPv6
      # there may be multiple ':' characters, and recall that regular
      # expressions are greedy. If grep has multiple matches this breaks,
      # which is Okay because then the emulator is exhibiting unexpected
      # behavior.
      emulator_port=$(grep "${expected}" "${logfile}" | sed 's/^.*://')
      break
    fi
    sleep 1
  done

  if [[ "${emulator_port}" = "0" ]]; then
    echo "Cannot determine Bigtable emulator port." >&2
    kill_emulators
    exit 1
  fi

  echo "${emulator_port}"
}

################################################
# Wait until it can successfully connect to the emulator.
# Globals:
#   None
# Arguments:
#   address: the expected address for the emulator
#   subcmd: the Cloud Bigtable command-line tool sub-command to test with.
# Returns:
#   None
################################################
function wait_until_emulator_connects() {
  local address=$1
  local subcmd=$2
  shift 2

  local -a CBT_ARGS=("-project" "emulated" "-instance" "emulated" "-creds" "default")
  # Wait until the emulator starts responding.
  delay=1
  connected=no
  local -r attempts=$(seq 1 8)
  for _ in ${attempts}; do
    if env BIGTABLE_EMULATOR_HOST="${address}" \
      "${CBT_CMD}" "${CBT_ARGS[@]}" "${subcmd}" >/dev/null 2>&1; then
      connected=yes
      break
    fi
    sleep $delay
    delay=$((delay * 2))
  done

  if [[ "${connected}" = "no" ]]; then
    echo "Cannot connect to Cloud Bigtable emulator; aborting test." >&2
    exit 1
  fi

  echo "Successfully connected to the Cloud Bigtable emulator on ${address}."
}

function start_emulators() {
  local -r initial_port="${1:-0}"
  local -r initial_instance_admin_port="${2:-0}"

  echo "Launching Cloud Bigtable emulators in the background"
  trap kill_emulators EXIT

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  "${CBT_EMULATOR_CMD}" -port "${initial_port}" >emulator.log 2>&1 </dev/null &
  EMULATOR_PID=$!
  local -r emulator_port="$(wait_for_port emulator.log)"

  "${CBT_INSTANCE_ADMIN_EMULATOR_CMD}" "${initial_instance_admin_port}" >instance-admin-emulator.log 2>&1 </dev/null &
  INSTANCE_ADMIN_EMULATOR_PID=$!
  local -r instance_admin_port="$(wait_for_port instance-admin-emulator.log)"

  wait_until_emulator_connects "localhost:${emulator_port}" "ls"
  wait_until_emulator_connects "localhost:${instance_admin_port}" "listinstances"

  export BIGTABLE_EMULATOR_HOST="localhost:${emulator_port}"
  export BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST="localhost:${instance_admin_port}"
}
