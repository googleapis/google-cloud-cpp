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

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/define-example-runner.sh"

function cleanup_instance {
  local project=$1
  local instance=$2
  shift 2

  echo
  echo "Cleaning up test instance projects/${project}/instances/${instance}"
  ./bigtable_instance_admin_snippets delete-instance "${project}" "${instance}"
}

function exit_handler {
  local project=$1
  local instance=$2
  shift 2

  if [[ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]]; then
    kill_emulators
  else
    cleanup_instance "${project}" "${instance}"
  fi
}

################################################
# Run the Bigtable quick start example.
# Globals:
#   None
# Arguments:
#   project_id: the Google Cloud Storage project used in the test. Can be a
#       fake project when testing against the emulator, as the emulator creates
#       projects on demand. It must be a valid, existing instance when testing
#       against production.
#   instance_id: the Google Cloud Bigtable instance used in the test. Can be a
#       fake instance when testing against the emulator, as the emulator creates
#       instances on demand. It must be a valid, existing instance when testing
#       against production.
# Returns:
#   None
################################################
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
run_quickstart_example() {
  local project_id=$1
  local instance_id=$2
  shift 2

  # Use the same table in all the tests.
  local -r TABLE="quickstart-tbl-${RANDOM}"

  # Run the example with an empty table, exercise the path where the row is
  # not found.
  run_example "${CBT_CMD}" -project "${project_id}" -instance "${instance_id}" \
      createtable "${TABLE}" "families=cf1"
  run_example ./bigtable_quickstart "${project_id}" "${instance_id}" "${TABLE}"

  # Use the Cloud Bigtable command-line tool to create a row, exercise the path
  # where the row is found.
  run_example "${CBT_CMD}" -project "${project_id}" -instance "${instance_id}" \
      set "${TABLE}" "r1" "cf1:greeting=Hello"
  run_example ./bigtable_quickstart "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_quickstart
}

################################################
# Run the Bigtable hello world for InstanceAdmin example.
# Globals:
#   None
# Arguments:
#   project_id: the Google Cloud Platform project used in the test. Can be a
#       fake project when testing against the emulator, as the emulator creates
#       projects on demand. It must be a valid, existing instance when testing
#       against production.
#   zone_id: a Google Cloud Platform zone with support for Cloud Bigtable.
# Returns:
#   None
################################################
run_hello_instance_admin_example() {
  local project_id=$1
  local zone_id=$2
  shift 2

  # Use the same table in all the tests.
  local -r RANDOM_INSTANCE_ID="it-${RANDOM}-${RANDOM}"
  local -r RANDOM_CLUSTER_ID="${RANDOM_INSTANCE_ID}-c1"

  run_example ./bigtable_hello_instance_admin \
      "${project_id}" "${RANDOM_INSTANCE_ID}" "${RANDOM_CLUSTER_ID}" \
      "${zone_id}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_hello_instance_admin
}
