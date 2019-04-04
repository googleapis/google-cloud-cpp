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
  ./bigtable_instance_admin_snippets delete-instance "${project}" "${instance}"
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

# Run all the instance admin examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_instance_admin_examples {
  local project_id=$1
  local zone_id=$2
  local replication_zone_id=$3
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  if [ -z "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    echo "Aborting the test as the instance admin examples should not run" \
        " against production."
    exit 1
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"
  local -r DEV_INSTANCE="in-${RANDOM}-${RANDOM}-dev"
  local -r RUN_INSTANCE="in-${RANDOM}-${RANDOM}-run"

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ./bigtable_instance_admin_snippets list-instances \
      "${project_id}"
  run_example ./bigtable_instance_admin_snippets get-instance \
      "${project_id}" "${INSTANCE}"
  run_example ./bigtable_instance_admin_snippets list-clusters \
      "${project_id}" "${INSTANCE}"
  run_example ./bigtable_instance_admin_snippets list-all-clusters \
      "${project_id}"
  run_example ./bigtable_instance_admin_snippets create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "${replication_zone_id}"
  run_example ./bigtable_instance_admin_snippets update-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets get-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets create-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./bigtable_instance_admin_snippets delete-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./bigtable_instance_admin_snippets \
      create-app-profile-cluster "${project_id}" "${INSTANCE}" "profile-c2" \
      "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets list-app-profiles \
      "${project_id}" "${INSTANCE}"
  run_example ./bigtable_instance_admin_snippets get-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"
  run_example ./bigtable_instance_admin_snippets \
       update-app-profile-description \
      "${project_id}" "${INSTANCE}" "profile-c2" "new-profile-description"
  run_example ./bigtable_instance_admin_snippets \
       update-app-profile-routing-any \
      "${project_id}" "${INSTANCE}" "profile-c2"
  run_example ./bigtable_instance_admin_snippets \
      update-app-profile-routing \
      "${project_id}" "${INSTANCE}" "profile-c2" "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets delete-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"
  run_example ./bigtable_instance_admin_snippets delete-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets get-iam-policy \
      "${project_id}" "${INSTANCE}"
  run_example ./bigtable_instance_admin_snippets set-iam-policy \
      "${project_id}" "${INSTANCE}" "roles/bigtable.user" "nobody@example.com"
  run_example ./bigtable_instance_admin_snippets test-iam-permissions \
      "${project_id}" "${INSTANCE}" "bigtable.instances.delete"
  run_example ./bigtable_instance_admin_snippets delete-instance \
      "${project_id}" "${INSTANCE}"

  run_example ./bigtable_instance_admin_snippets create-dev-instance \
      "${project_id}" "${DEV_INSTANCE}" "${zone_id}"
  run_example ./bigtable_instance_admin_snippets update-instance \
      "${project_id}" "${DEV_INSTANCE}"
  run_example ./bigtable_instance_admin_snippets delete-instance \
      "${project_id}" "${DEV_INSTANCE}"

  run_example ./bigtable_instance_admin_snippets run \
      "${project_id}" "${RUN_INSTANCE}" "${RUN_INSTANCE}-c1" "${zone_id}"

  run_example ./bigtable_samples_instance_admin run \
      "${project_id}" "${RUN_INSTANCE}" "${RUN_INSTANCE}-c1" "${zone_id}"

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ./bigtable_samples_instance_admin create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "${replication_zone_id}"
  run_example ./bigtable_samples_instance_admin delete-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./bigtable_samples_instance_admin delete-instance \
      "${project_id}" "${INSTANCE}"

  run_example ./bigtable_samples_instance_admin \
      create-dev-instance \
      "${project_id}" "${DEV_INSTANCE}" "${INSTANCE}-c1" "${zone_id}"
  run_example ./bigtable_samples_instance_admin delete-instance \
      "${project_id}" "${DEV_INSTANCE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_instance_admin_snippets
  run_example_usage ./bigtable_samples_instance_admin
}

# Run all the instance admin async examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_instance_admin_async_examples {
  local project_id=$1
  local zone_id=$2
  local replication_zone_id=$3
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  if [ -z "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    echo "Aborting the test as the instance admin examples should not run" \
        " against production."
    exit 1
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ./instance_admin_async_snippets async-get-instance \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-list-instances \
      "${project_id}"
  run_example ./instance_admin_async_snippets async-list-clusters \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-list-all-clusters \
      "${project_id}"
  run_example ./bigtable_instance_admin_snippets create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "${replication_zone_id}"
  run_example ./instance_admin_async_snippets async-get-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./bigtable_instance_admin_snippets delete-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./instance_admin_async_snippets async-delete-instance \
      "${project_id}" "${INSTANCE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./instance_admin_async_snippets
}

# Run all the table admin examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_table_admin_examples {
  local project_id=$1
  local zone_id=$2
  shift 2

  EMULATOR_LOG="emulator.log"

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"

  # Use the same table in all the tests.
  local -r TABLE="sample-table-for-admin-${RANDOM}"

  run_example ./bigtable_samples run "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets create-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets list-tables "${project_id}" "${INSTANCE}"
  run_example ./table_admin_snippets get-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./data_snippets bulk-apply "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets modify-table "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets wait-for-consistency-check "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets generate-consistency-token "${project_id}" "${INSTANCE}" "${TABLE}"
  local token="$(./table_admin_snippets generate-consistency-token ${project_id} ${INSTANCE} ${TABLE} | awk '{print $5}')"
  run_example ./table_admin_snippets check-consistency "${project_id}" "${INSTANCE}" "${TABLE}" "${token}"
  run_example ./table_admin_snippets drop-rows-by-prefix "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./data_snippets read-rows "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets drop-all-rows "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets delete-table "${project_id}" "${INSTANCE}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./table_admin_snippets
}

# Run all the table admin async examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_table_admin_async_examples {
  local project_id=$1
  local zone_id=$2
  shift 2

  EMULATOR_LOG="emulator.log"

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"

  # Use the same table in all the tests.
  local -r TABLE="sample-table-for-admin-${RANDOM}"

  run_example ./table_admin_async_snippets async-create-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-get-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-modify-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-delete-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./table_admin_async_snippets
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
  local project_id=$1
  local instance_id=$2
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  # Use a different table for the full example test, if we use the same table
  # as the other tests this can fail with timeouts.
  local -r FULL_TABLE="data-ex-full-${RANDOM}-${RANDOM}"
  run_example ./bigtable_samples run-full-example "${project_id}" "${instance_id}" "${FULL_TABLE}"

  # Use the same table in all the tests.
  local -r TABLE="data-ex-tbl-${RANDOM}-${RANDOM}"
  local -r PREFIX_1="root/0/1/"
  local -r PREFIX_2="root/2/1/"
  local -r ROW_KEY_1="root/0/0/1"
  local -r ROW_KEY_2="root/0/1/0"
  local -r ROW_KEY_3="key-000007"
  local -r COLUMN_NAME="col2"
  local -r FAMILY_NAME="fam"

  run_example ./table_admin_snippets create-table "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets bulk-apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-rows-with-limit "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-rows "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets populate-table-hierarchy "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-rowset "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_1}" "${ROW_KEY_2}"
  run_example ./data_snippets read-rowset-prefix "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}"
  run_example ./data_snippets read-prefix-list "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}" "${PREFIX_2}"
  run_example ./data_snippets sample-rows "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets sample-rows-collections "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-modify-write "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-row "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets get-family "${project_id}" "${instance_id}" "${TABLE}"  
  run_example ./data_snippets delete-all-cells "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_1}"
  run_example ./data_snippets delete-family-cells \
  "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_2}" "${FAMILY_NAME}"
  run_example ./data_snippets delete-selective-family-cells \
  "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_2}" "${FAMILY_NAME}" "${COLUMN_NAME}"
  run_example ./table_admin_snippets delete-table "${project_id}" "${instance_id}" "${TABLE}"
}

function run_all_data_async_examples {
  local project_id=$1
  local instance_id=$2
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  # Use the same table in all the tests.
  local -r TABLE="data-ex-tbl-${RANDOM}-${RANDOM}"
  run_example ./table_admin_snippets create-table "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-bulk-apply "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-check-and-mutate "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./table_admin_snippets delete-table "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./data_async_snippets
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
  run_example ${CBT_CMD} -project "${project_id}" -instance "${instance_id}" \
      createtable "${TABLE}" "families=cf1"
  run_example ./bigtable_quickstart "${project_id}" "${instance_id}" "${TABLE}"

  # Use the Cloud Bigtable command-line tool to create a row, exercise the path
  # where the row is found.
  run_example ${CBT_CMD} -project "${project_id}" -instance "${instance_id}" \
      set "${TABLE}" "r1" "cf1:greeting=Hello"
  run_example ./bigtable_quickstart "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_quickstart
}

################################################
# Run the Bigtable hello world example.
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
run_hello_world_example() {
  local project_id=$1
  local instance_id=$2
  shift 2

  # Use the same table in all the tests.
  local -r TABLE="hello-world-tbl-${RANDOM}"

  run_example ./bigtable_hello_world "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_hello_world
}
################################################
# Run the Bigtable hello app profile example.
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
run_hello_app_profile_example() {
  local project_id=$1
  local instance_id=$2
  shift 2

  # Use the same table in all the tests.
  local -r TABLE="hello-app-profile-tbl-${RANDOM}"
  local -r PROFILE_ID="profile-${RANDOM}"

  run_example ./table_admin_snippets create-table \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./bigtable_hello_app_profile \
      "${project_id}" "${instance_id}" "${TABLE}" "${PROFILE_ID}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_hello_app_profile
}
