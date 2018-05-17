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

readonly CBT_CMD="${CBT:-${GOPATH}/bin/cbt}"
readonly CBT_EMULATOR_CMD="${CBT_EMULATOR:-${GOPATH}/bin/emulator}"
readonly CBT_INSTANCE_ADMIN_EMULATOR_CMD="../tests/instance_admin_emulator"

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

  local admin="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    admin="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  echo
  echo "Run create-instance example."
  $admin ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}"

  echo
  echo "Run list-instances example."
  $admin ../examples/bigtable_samples_instance_admin list-instances "${project_id}"

  echo
  echo "Run get-instance example."
  $admin ../examples/bigtable_samples_instance_admin get-instance "${project_id}" "${INSTANCE}"

#  TODO(#490) - disabled until ListClusters works correctly.
#  echo
#  echo "Run list-clusters example."
#  $admin ../examples/bigtable_samples_instance_admin list-clusters "${project_id}"

  echo
  echo "Run delete-instance example."
  $admin ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}"
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

  local admin="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    admin="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Use the same table in all the tests.
  local -r TABLE="sample-table"

  # Create an instance to run these examples.
  $admin ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}"

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

  $admin ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}"
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

  local admin="env"
  if [ -n "${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST:-}" ]; then
    admin="env BIGTABLE_EMULATOR_HOST=${BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST}"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Use the same table in all the tests.
  local -r TABLE="sample-table"

  # Create an instance to run these examples.
  $admin ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}"

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

  $admin ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}"
}
