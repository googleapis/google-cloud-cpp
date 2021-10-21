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

set -eu

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module ci/lib/io.sh

EMULATOR_PID=0
INSTANCE_ADMIN_EMULATOR_PID=0

function kill_emulators() {
  io::log "Killing Bigtable Emulators..."
  echo -n "EMULATOR_PID=${EMULATOR_PID} "
  kill "${EMULATOR_PID}" || echo -n "-"
  echo -n "."
  wait "${EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo "."
  echo -n "INSTANCE_ADMIN_EMULATOR_PID=${INSTANCE_ADMIN_EMULATOR_PID} "
  kill "${INSTANCE_ADMIN_EMULATOR_PID}" || echo -n "-"
  echo -n "."
  wait "${INSTANCE_ADMIN_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo "."
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

  local cmd=(
    "/usr/local/google-cloud-sdk/bin/cbt"
    "-project" "emulated"
    "-instance" "emulated"
    "-creds" "default"
    "${subcmd}"
  )

  # Wait until the emulator starts responding.
  delay=1
  for _ in $(seq 1 8); do
    if env BIGTABLE_EMULATOR_HOST="${address}" "${cmd[@]}" >/dev/null 2>&1; then
      io::log_green "Connected to the Cloud Bigtable emulator on ${address}"
      return
    fi
    io::log "waiting for emulator to start on ${address}..."
    sleep $delay
    delay=$((delay * 2))
  done

  io::log_red "Cannot connect to Cloud Bigtable emulator; aborting test." >&2
  exit 1
}

function start_emulators() {
  local -r emulator_port="$1"
  local -r instance_admin_emulator_port="$2"

  io::log "Launching Cloud Bigtable emulators in the background"
  trap kill_emulators EXIT

  local -r CBT_EMULATOR_CMD="/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator"
  "${CBT_EMULATOR_CMD}" -port "${emulator_port}" >emulator.log 2>&1 </dev/null &
  EMULATOR_PID=$!

  # The path to this command must be set by the caller.
  "${CBT_INSTANCE_ADMIN_EMULATOR_CMD}" "${instance_admin_emulator_port}" >instance-admin-emulator.log 2>&1 </dev/null &
  INSTANCE_ADMIN_EMULATOR_PID=$!

  wait_until_emulator_connects "localhost:${emulator_port}" "ls"
  wait_until_emulator_connects "localhost:${instance_admin_emulator_port}" "listinstances"

  export BIGTABLE_EMULATOR_HOST="localhost:${emulator_port}"
  export BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST="localhost:${instance_admin_emulator_port}"
}
