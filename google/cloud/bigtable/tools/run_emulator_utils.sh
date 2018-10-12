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

function kill_emulators {
  echo -n "Killing Bigtable Emulators [${EMULATOR_PID} ${INSTANCE_ADMIN_EMULATOR_PID}] "
  kill "${EMULATOR_PID}" || echo -n "-"
  echo -n "."
  wait "${EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo -n "."
  kill "${INSTANCE_ADMIN_EMULATOR_PID}"  || echo -n "-"
  echo -n "."
  wait "${INSTANCE_ADMIN_EMULATOR_PID}" >/dev/null 2>&1 || echo -n "+"
  echo -n "."
  echo " done."
}

function start_emulators {
  echo "Launching Cloud Bigtable emulators in the background"
  trap kill_emulators EXIT

  # The tests typically run in a Docker container, where the ports are largely
  # free; when using in manual tests, you can set EMULATOR_PORT.
  local -r emulator_port=${EMULATOR_PORT:-9000}
  "${CBT_EMULATOR_CMD}" -port "${emulator_port}" >emulator.log 2>&1 </dev/null &
  EMULATOR_PID=$!

  local -r instance_admin_port=${INSTANCE_ADMIN_EMULATOR_PORT:-9090}
  "${CBT_INSTANCE_ADMIN_EMULATOR_CMD}" "${instance_admin_port}" >instance-admin-emulator.log 2>&1 </dev/null &
  INSTANCE_ADMIN_EMULATOR_PID=$!

  export BIGTABLE_EMULATOR_HOST="localhost:${emulator_port}"
  # Avoid repetition
  readonly CBT_ARGS="-project emulated -instance emulated -creds default"
  # Wait until the emulator starts responding.
  delay=1
  connected=no
  local -r attempts=$(seq 1 8)
  for attempt in ${attempts}; do
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
  fi

  echo "Successfully connected to the Cloud Bigtable emulator."

  export BIGTABLE_EMULATOR_HOST="localhost:${instance_admin_port}"
  delay=1
  connected=no
  for attempt in ${attempts}; do
    if "${CBT_CMD}" $CBT_ARGS listinstances >/dev/null 2>&1; then
      connected=yes
      break
    fi
    sleep $delay
    delay=$((delay * 2))
  done

  if [ "${connected}" = "no" ]; then
    echo "Cannot connect to Cloud Bigtable Instance Admin emulator; aborting test." >&2
    exit 1
  fi
  echo "Successfully connected to the Cloud Bigtable Instance Admin emulator."

  export BIGTABLE_EMULATOR_HOST="localhost:${emulator_port}"
  export BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST="localhost:${instance_admin_port}"
}
