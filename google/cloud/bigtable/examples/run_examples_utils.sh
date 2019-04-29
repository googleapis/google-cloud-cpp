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

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"
  local -r DEV_INSTANCE="in-${RANDOM}-${RANDOM}-dev"
  local -r RUN_INSTANCE="in-${RANDOM}-${RANDOM}-run"
  local -r REPLICATED_INSTANCE="in-${RANDOM}-${RANDOM}-rep"

  run_example ./bigtable_instance_admin_snippets create-replicated-instance \
      "${project_id}" "${REPLICATED_INSTANCE}" \
      "${zone_id}" "${replication_zone_id}"
  run_example ./bigtable_instance_admin_snippets get-instance \
      "${project_id}" "${REPLICATED_INSTANCE}"
  run_example ./bigtable_instance_admin_snippets delete-instance \
      "${project_id}" "${REPLICATED_INSTANCE}"

  run_failure_example ./bigtable_instance_admin_snippets check-instance-exists \
      "${project_id}" "${INSTANCE}"
  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ./bigtable_instance_admin_snippets check-instance-exists \
      "${project_id}" "${INSTANCE}"
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
      "${project_id}" "${INSTANCE}" "roles/bigtable.user" \
      "serviceAccount:${SERVICE_ACCOUNT}"
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

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_instance_admin_snippets
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

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-${RANDOM}-${RANDOM}"

  run_example ./instance_admin_async_snippets async-create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  run_example ./instance_admin_async_snippets async-update-instance \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-get-instance \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-list-instances \
      "${project_id}"
  run_example ./instance_admin_async_snippets async-list-clusters \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-list-all-clusters \
      "${project_id}"
  run_example ./instance_admin_async_snippets async-list-app-profiles \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "${replication_zone_id}"
  run_example ./instance_admin_async_snippets async-update-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./instance_admin_async_snippets async-get-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./instance_admin_async_snippets async-delete-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2"
  run_example ./instance_admin_async_snippets async-create-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./instance_admin_async_snippets async-get-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./instance_admin_async_snippets async-update-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./instance_admin_async_snippets async-delete-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"
  run_example ./instance_admin_async_snippets async-get-iam-policy \
      "${project_id}" "${INSTANCE}"
  run_example ./instance_admin_async_snippets async-set-iam-policy \
      "${project_id}" "${INSTANCE}" "roles/bigtable.user" \
      "serviceAccount:${SERVICE_ACCOUNT}"
  run_example ./instance_admin_async_snippets async-test-iam-permissions \
      "${project_id}" "${INSTANCE}" "bigtable.instances.delete"
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

  # Use the same table in most of the tests.
  local -r TABLE="sample-table-for-admin-${RANDOM}"
  local -r TABLE2="sample-table-for-admin-${RANDOM}"

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"

  run_example ./table_admin_snippets create-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets list-tables \
      "${project_id}" "${INSTANCE}"
  run_example ./table_admin_snippets get-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets check-table-exists \
      "${project_id}" "${INSTANCE}" "${TABLE}"

  run_failure_example ./table_admin_snippets check-table-exists \
      "${project_id}" "${INSTANCE}" "${TABLE2}"
  run_example ./table_admin_snippets get-or-create-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets get-or-create-table \
      "${project_id}" "${INSTANCE}" "${TABLE2}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${INSTANCE}" "${TABLE2}"

  # Insert some data into the table, before changing the schema to show that it
  # is possible to do that.
  run_example ./data_snippets bulk-apply \
      "${project_id}" "${INSTANCE}" "${TABLE}"

  run_example ./table_admin_snippets modify-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets create-max-age-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "max-age-family"
  run_example ./table_admin_snippets create-max-versions-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "max-versions-family"
  run_example ./table_admin_snippets create-union-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "union-family"
  run_example ./table_admin_snippets create-intersection-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "intersection-family"
  run_example ./table_admin_snippets create-nested-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "nested-family"
  run_example ./table_admin_snippets list-column-families \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets get-family-metadata \
      "${project_id}" "${INSTANCE}" "${TABLE}" "nested-family"
  run_example ./table_admin_snippets get-or-create-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "get-or-create-family"
  run_example ./table_admin_snippets delete-column-family \
      "${project_id}" "${INSTANCE}" "${TABLE}" "nested-family"
  run_failure_example ./table_admin_snippets check-family-exists \
      "${project_id}" "${INSTANCE}" "${TABLE}" "nested-family"
  run_example ./table_admin_snippets check-family-exists \
      "${project_id}" "${INSTANCE}" "${TABLE}" "max-age-family"
  run_example ./table_admin_snippets update-gc-rule \
      "${project_id}" "${INSTANCE}" "${TABLE}" "max-age-family"

  run_example ./table_admin_snippets wait-for-consistency-check \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets generate-consistency-token \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  local token
  token="$(./table_admin_snippets generate-consistency-token \
      "${project_id}" "${INSTANCE}" "${TABLE}" | awk '{print $5}')"
  run_example ./table_admin_snippets check-consistency \
      "${project_id}" "${INSTANCE}" "${TABLE}" "${token}"

  # bulk-apply above generates a number of keys, all starting with key-%06d, so
  # key-0001 is a good prefix to test.
  run_example ./table_admin_snippets drop-rows-by-prefix \
      "${project_id}" "${INSTANCE}" "${TABLE}" "key-0001"
  run_example ./data_snippets read-rows \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets drop-all-rows \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"

  run_example ./bigtable_instance_admin_snippets delete-instance \
      "${project_id}" "${INSTANCE}"

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

  # Use sample row key wherever needed.
  local -r ROW_KEY="sample-row-key-${RANDOM}"

  run_example ./bigtable_instance_admin_snippets create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"

  run_example ./table_admin_async_snippets async-create-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-list-tables \
      "${project_id}" "${INSTANCE}"
  run_example ./table_admin_async_snippets async-get-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-modify-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-generate-consistency-token \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  local token
  token="$(./table_admin_async_snippets async-generate-consistency-token \
      "${project_id}" "${INSTANCE}" "${TABLE}" | awk '{print $5}')"
  run_example ./table_admin_async_snippets async-check-consistency \
      "${project_id}" "${INSTANCE}" "${TABLE}" "${token}"
  run_example ./table_admin_async_snippets async-wait-for-consistency \
      "${project_id}" "${INSTANCE}" "${TABLE}" "${token}"
  run_example ./table_admin_async_snippets async-drop-rows-by-prefix \
      "${project_id}" "${INSTANCE}" "${TABLE}" "${ROW_KEY}"
  run_example ./table_admin_async_snippets async-drop-all-rows \
      "${project_id}" "${INSTANCE}" "${TABLE}"
  run_example ./table_admin_async_snippets async-delete-table \
      "${project_id}" "${INSTANCE}" "${TABLE}"

  run_example ./bigtable_instance_admin_snippets delete-instance \
      "${project_id}" "${INSTANCE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./table_admin_async_snippets
}

################################################
# Run the Bigtable mutate-rows examples.
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
run_mutate_examples() {
  local project_id=$1
  local instance_id=$2
  shift 2

  local -r TABLE="mutate-tbl-${RANDOM}-${RANDOM}"
  run_example ./table_admin_snippets create-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  # create-table creates a table with this family, just re-use it instead of
  # creating a new one.
  local -r FAMILY_NAME="fam"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "row1" "fam:col1=value1.1" "fam:col2=value1.2" "fam:col3=value1.3"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "row2" "fam:col1=value2.1" "fam:col2=value2.2" "fam:col3=value2.3"

  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"
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

  EMULATOR_LOG="emulator.log"

  run_mutate_examples "${project_id}" "${instance_id}"

  # Use the same table in all the tests.
  local -r TABLE="data-ex-tbl-${RANDOM}-${RANDOM}"
  local -r PREFIX_1="root/0/1/"
  local -r PREFIX_2="root/2/1/"
  local -r ROW_KEY_1="root/0/0/1"
  local -r ROW_KEY_2="root/0/1/0"
  local -r ROW_KEY_3="key-000007"
  local -r COLUMN_NAME="col2"
  local -r FAMILY_NAME="fam"

  run_example ./table_admin_snippets create-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "insert-update-01" \
      "fam:col0=value0-0" "fam:col1=value2-0" \
      "fam:col3=value3-0" "fam:col4=value4-0"
  run_example ./data_snippets mutate-delete-columns \
      "${project_id}" "${instance_id}" "${TABLE}" "insert-update-01" \
      "fam:col3" "fam:col4"
  run_example ./data_snippets mutate-delete-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "insert-update-01"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "to-delete-01" \
      "fam:col0=value0-0" "fam:col1=value2-0" \
      "fam:col3=value3-0" "fam:col4=value4-0"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "to-delete-02" \
      "fam:col0=value0-0" "fam:col1=value2-0" \
      "fam:col3=value3-0" "fam:col4=value4-0"
  run_example ./data_snippets mutate-delete-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "to-delete-01" "to-delete-02"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "mix-match-01" \
      "fam:col0=value0-0"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "mix-match-01" \
      "fam:col0=value0-1"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "mix-match-01" \
      "fam:col0=value0-2"
  run_example ./data_snippets rename-column \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "mix-match-01" "fam" "col0" "new-name"

  run_example ./data_snippets insert-test-data \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets apply \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets apply-relaxed-idempotency \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "apply-relaxed-idempotency"
  run_example ./data_snippets apply-custom-retry \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "apply-custom-retry"
  run_example ./data_snippets bulk-apply \
      "${project_id}" "${instance_id}" "${TABLE}"

  run_example ./data_snippets read-rows-with-limit \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-rows \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets populate-table-hierarchy \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets read-keys-set \
      "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_1}" "${ROW_KEY_2}"
  run_example ./data_snippets read-rowset-prefix \
      "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}"
  run_example ./data_snippets read-prefix-list \
      "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}" "${PREFIX_2}"
  run_example ./data_snippets read-multiple-ranges \
      "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}" "${PREFIX_2}"
  run_example ./data_snippets read-multiple-ranges \
      "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}" "${PREFIX_2}" \
      "key-000007" "key-000009"
  run_failure_example ./data_snippets read-multiple-ranges \
      "${project_id}" "${instance_id}" "${TABLE}" "${PREFIX_1}" "${PREFIX_2}" \
      "mismatched-begin-end-pair"
  run_example ./data_snippets sample-rows \
      "${project_id}" "${instance_id}" "${TABLE}"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "check-and-mutate-row" \
      "fam:flip-flop=on"
  run_example ./data_snippets check-and-mutate \
      "${project_id}" "${instance_id}" "${TABLE}" "check-and-mutate-row"
  run_example ./data_snippets check-and-mutate \
      "${project_id}" "${instance_id}" "${TABLE}" "check-and-mutate-row"

  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "check-and-mutate-not-present-row" "fam:unused=unused-value"
  run_example ./data_snippets check-and-mutate-not-present \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "check-and-mutate-not-present-row"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "check-and-mutate-not-present-row" "fam:test-column=unused-value"
  run_example ./data_snippets check-and-mutate-not-present \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "check-and-mutate-not-present-row"

  local -r READ_ROW_KEY="read-row-${RANDOM}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"
  run_example ./data_snippets mutate-insert-update-rows \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}" \
      "fam:flip-flop=on"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"
  run_example ./data_snippets check-and-mutate \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"
  run_example ./data_snippets check-and-mutate \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_ROW_KEY}"

  local -r READ_MODIFY_WRITE_KEY="read-modify-write-${RANDOM}"
  run_example ./data_snippets read-modify-write \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"
  run_example ./data_snippets read-modify-write \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"
  run_example ./data_snippets read-modify-write \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"
  run_example ./data_snippets read-row \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_KEY}"

  run_example ./data_snippets row-exists \
      "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_1}"
  run_example ./data_snippets get-family \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_snippets delete-all-cells \
      "${project_id}" "${instance_id}" "${TABLE}" "${ROW_KEY_1}"
  run_example ./data_snippets delete-family-cells \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "${ROW_KEY_2}" "${FAMILY_NAME}"
  run_example ./data_snippets delete-selective-family-cells \
      "${project_id}" "${instance_id}" "${TABLE}" \
      "${ROW_KEY_2}" "${FAMILY_NAME}" "${COLUMN_NAME}"

  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"
}

################################################
# Run the Bigtable examples for Async* operations on data.
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
function run_all_data_async_examples {
  local project_id=$1
  local instance_id=$2
  shift 2

  EMULATOR_LOG="instance-admin-emulator.log"

  # Use the same table in all the tests.
  local -r TABLE="data-ex-tbl-${RANDOM}-${RANDOM}"
  local -r APPLY_ROW_KEY="async-apply-row-${RANDOM}"
  local -r CHECK_AND_MUTATE_ROW_KEY="check-and-mutate-row-${RANDOM}"
  local -r READ_MODIFY_WRITE_ROW_KEY="read-modify-write-row-${RANDOM}"
  run_example ./table_admin_snippets create-table \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-apply \
      "${project_id}" "${instance_id}" "${TABLE}" "${APPLY_ROW_KEY}"
  run_example ./data_async_snippets async-bulk-apply \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-read-rows \
      "${project_id}" "${instance_id}" "${TABLE}"
  run_example ./data_async_snippets async-read-rows-with-limit \
      "${project_id}" "${instance_id}" "${TABLE}"

  run_example ./data_async_snippets async-apply \
      "${project_id}" "${instance_id}" "${TABLE}" "${CHECK_AND_MUTATE_ROW_KEY}"
  run_example ./data_async_snippets async-check-and-mutate \
      "${project_id}" "${instance_id}" "${TABLE}" "${CHECK_AND_MUTATE_ROW_KEY}"

  run_example ./data_async_snippets async-apply \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_ROW_KEY}"
  run_example ./data_async_snippets async-read-modify-write \
      "${project_id}" "${instance_id}" "${TABLE}" "${READ_MODIFY_WRITE_ROW_KEY}"

  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"

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

################################################
# Run the Bigtable hello world for Table Admin example.
# Globals:
#   None
# Arguments:
#   project_id: the Google Cloud Platform project used in the test. Can be a
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
run_hello_table_admin_example() {
  local project_id=$1
  local instance_id=$2
  shift 2

  # Use the same table in all the tests.
  local -r TABLE="hello-table-admin-${RANDOM}"
  run_example ./bigtable_hello_table_admin \
      "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_hello_table_admin
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
  run_example ./bigtable_instance_admin_snippets create-app-profile \
      "${project_id}" "${instance_id}" "${PROFILE_ID}"
  run_example ./bigtable_hello_app_profile \
      "${project_id}" "${instance_id}" "${TABLE}" "${PROFILE_ID}"
  run_example ./bigtable_instance_admin_snippets delete-app-profile \
      "${project_id}" "${instance_id}" "${PROFILE_ID}"
  run_example ./table_admin_snippets delete-table \
      "${project_id}" "${instance_id}" "${TABLE}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./bigtable_hello_app_profile
}
