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

  local setenv="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    setenv="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  echo
  echo "Run create-instance example."
  ${setenv} ../examples/bigtable_samples_instance_admin create-instance \
      "${project_id}" "${INSTANCE}" "${zone_id}"
  trap 'exit_handler "${project_id}" "${INSTANCE}"' EXIT

  echo
  echo "Run list-instances example."
  ${setenv} ../examples/bigtable_samples_instance_admin list-instances \
      "${project_id}"

  echo
  echo "Run get-instance example."
  ${setenv} ../examples/bigtable_samples_instance_admin get-instance \
      "${project_id}" "${INSTANCE}"

  echo
  echo "Run list-clusters example."
  ${setenv} ../examples/bigtable_samples_instance_admin list-clusters \
      "${project_id}" "${INSTANCE}"

  echo
  echo "Run list-all-clusters example."
  ${setenv} ../examples/bigtable_samples_instance_admin list-all-clusters \
      "${project_id}"

  echo
  echo "Run create cluster example."
  ${setenv} ../examples/bigtable_samples_instance_admin create-cluster \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c2" "us-central1-a"

  echo
  echo "Run create-app-profile example."
  ${setenv} ../examples/bigtable_samples_instance_admin create-app-profile \
      "${project_id}" "${INSTANCE}" "my-profile"

  echo
  echo "Run create-app-profile-cluster example."
  ${setenv} ../examples/bigtable_samples_instance_admin \
      create-app-profile-cluster "${project_id}" "${INSTANCE}" "profile-c2" \
      "${INSTANCE}-c2"

  echo
  echo "Run list-app-profile example."
  ${setenv} ../examples/bigtable_samples_instance_admin list-app-profiles \
      "${project_id}" "${INSTANCE}"

  echo
  echo "Run get-app-profile example."
  ${setenv} ../examples/bigtable_samples_instance_admin get-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"

  echo
  echo "Run delete-app-profile example."
  ${setenv} ../examples/bigtable_samples_instance_admin delete-app-profile \
      "${project_id}" "${INSTANCE}" "profile-c2"

  echo
  echo "Run get-iam-policy example."
  ${setenv} ../examples/bigtable_samples_instance_admin get-iam-policy \
      "${project_id}" "${INSTANCE}"

  echo
  echo "Run set-iam-policy example."
  ${setenv} ../examples/bigtable_samples_instance_admin set-iam-policy \
      "${project_id}" "${INSTANCE}" "roles/bigtable.user" "nobody@example.com"

  echo
  echo "Run test-iam-permissions example."
  ${setenv} ../examples/bigtable_samples_instance_admin test-iam-permissions \
      "${project_id}" "${INSTANCE}" "bigtable.instances.delete"

  echo
  echo "Run delete-instance example."
  cleanup_instance "${project_id}" "${INSTANCE}"

  echo
  echo "Run example for basic instance operations"
    ${setenv} ../examples/bigtable_samples_instance_admin run \
      "${project_id}" "${INSTANCE}" "${INSTANCE}-c1" "${zone_id}"
  trap 'exit_handler "${project_id}" "${INSTANCE}"' EXIT

  reset_trap
  echo
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

  local setenv="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    setenv="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Use the same table in all the tests.
  local -r TABLE="sample-table"

  # Create an instance to run these examples.
  ${setenv} ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}"
  trap 'exit_handler "${project_id}" "${INSTANCE}"' EXIT

  echo
  echo "Run all basic table operations example."
  ../examples/bigtable_samples run "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run create-table example."
  ../examples/bigtable_samples create-table "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run list-tables example."
  ../examples/bigtable_samples list-tables "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run get-table example."
  ../examples/bigtable_samples get-table "${project_id}" "${INSTANCE}" "${TABLE}"

  # Populate some data on the table, so the next examples are meaningful.
  ../examples/bigtable_samples bulk-apply "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run modify-table example."
  ../examples/bigtable_samples modify-table "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run drop-rows-by-prefix example."
  ../examples/bigtable_samples drop-rows-by-prefix "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run drop-all-rows example."
  ../examples/bigtable_samples drop-all-rows "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run delete-table example."
  ../examples/bigtable_samples delete-table "${project_id}" "${INSTANCE}" "${TABLE}"

  reset_trap
  cleanup_instance "${project_id}" "${INSTANCE}"
}

# Run the Bigtable data manipulation examples.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_data_examples {
  if [ ! -x ../examples/bigtable_samples ]; then
    echo "Will not run the examples as the examples were not built"
    return
  fi

  local project_id=$1
  local zone_id=$2
  shift 2

  local setenv="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    setenv="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Use the same table in all the tests.
  local -r TABLE="sample-table"

  # Create an instance to run these examples.
  ${setenv} ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}"
  trap 'exit_handler "${project_id}" "${INSTANCE}"' EXIT

  echo
  echo "Run create-table example."
  ../examples/bigtable_samples create-table "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run apply example."
  ../examples/bigtable_samples apply "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run bulk-apply example."
  ../examples/bigtable_samples bulk-apply "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run read-row example."
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run read-rows-with-limit example."
  ../examples/bigtable_samples read-rows-with-limit "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run scan/read-rows example."
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run sample-rows example."
  ../examples/bigtable_samples sample-rows "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run check-and-mutate example."
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"

  echo
  echo "Run read-modify-write example."
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" "${TABLE}"
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" "${TABLE}"

  reset_trap
  cleanup_instance "${project_id}" "${INSTANCE}"
}
