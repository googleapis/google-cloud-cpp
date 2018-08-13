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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/define-example-runner.sh"

function cleanup_instance {
  local project=$1
  local instance=$2
  shift 2

  echo
  echo "Cleaning up test instance projects/${project}/instances/${instance}"
  local setenv="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    setenv="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi
  ${setenv} ../examples/bigtable_samples_instance_admin delete-instance "${project}" "${instance}"
}

function exit_handler {
  local project=$1
  local instance=$2
  shift 2

  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    kill_emulators
  else
    cleanup_instance "${project}" "${instance}"
  fi
}

# When we finish running a series of examples we want to explicitly cleanup the
# instance.  We cannot just let the exit handler do it because when running
# on the emulator the exit handler would kill the emulator.  And when running
# in production we create a different instance in each group of examples, and
# there is only one trap at a time.
function reset_trap {
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    # If the test is running against the emulator there is no need to cleanup
    # the instance, just kill the emulators.
    trap kill_emulators EXIT
  else
    trap - EXIT
  fi
}

# Run all the instance admin examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_instance_admin_examples {
  if [ ! -x ../examples/bigtable_samples_instance_admin ]; then
    echo "Will not run the examples as the examples were not built"
    return
  fi

  local project_id=$1
  local zone_id=$2
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  if [ -z "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    echo "Aborting the test as the instance admin examples should not run" \
        " against production."
    exit 1
  fi
  export BIGTABLE_EMULATOR_HOST="${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  run_example ../examples/bigtable_samples_instance_admin create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ../examples/bigtable_samples_instance_admin list-instances \
      "${project_id}"
  run_example ../examples/bigtable_samples_instance_admin get-instance \
      "${project_id}" "${INSTANCE}"
  run_example ../examples/bigtable_samples_instance_admin list-clusters \
      "${project_id}" "${INSTANCE}"
  run_example ../examples/bigtable_samples_instance_admin list-all-clusters \
      "${project_id}"
  run_example ../examples/bigtable_samples_instance_admin create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "us-central1-a"
  run_example ../examples/bigtable_samples_instance_admin create-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ../examples/bigtable_samples_instance_admin \
      create-app-profile-cluster "${project_id}" "${INSTANCE}" "profile-c2" \
      "${INSTANCE}-c2"
  run_example ../examples/bigtable_samples_instance_admin list-app-profiles \
      "${project_id}" "${INSTANCE}"
  run_example ../examples/bigtable_samples_instance_admin get-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"
  run_example ../examples/bigtable_samples_instance_admin delete-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"
  run_example ../examples/bigtable_samples_instance_admin get-iam-policy \
      "${project_id}" "${INSTANCE}"
  run_example ../examples/bigtable_samples_instance_admin set-iam-policy \
      "${project_id}" "${INSTANCE}" "roles/bigtable.user" "nobody@example.com"
  run_example ../examples/bigtable_samples_instance_admin test-iam-permissions \
      "${project_id}" "${INSTANCE}" "bigtable.instances.delete"

  cleanup_instance "${project_id}" "${INSTANCE}"

  run_example ../examples/bigtable_samples_instance_admin run \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c1" "${zone_id}"
}

# Run all the table admin examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_table_admin_examples {
  if [ ! -x ../examples/bigtable_samples ]; then
    echo "Will not run the examples as the examples were not built"
    return
  fi

  local project_id=$1
  local zone_id=$2
  shift 2

  EMULATOR_LOG="emulator.log"

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Use the same table in all the tests.
  local -r TABLE="sample-table-for-admin"

  run_example ../examples/bigtable_samples run "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples create-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples list-tables "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples get-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples bulk-apply "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples modify-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples drop-rows-by-prefix "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples drop-all-rows "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ../examples/bigtable_samples delete-table "${project_id}" "${INSTANCE}" "${TABLE}"
}

################################################
# Run the Bigtable data manipulation examples.
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
run_all_data_examples() {
  if [ ! -x ../examples/bigtable_samples ]; then
    echo "Will not run the examples as the examples were not built"
    return
  fi

  local project_id=$1
  local instance_id=$2
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  # Use the same table in all the tests.
  local -r TABLE="data-examples-tbl-${RANDOM}"

  run_example ../examples/bigtable_samples create-table "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples bulk-apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-rows-with-limit "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples scan "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples sample-rows "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ../examples/bigtable_samples read-row "${project_id}" "${instance_id}" "${TABLE}"
}
