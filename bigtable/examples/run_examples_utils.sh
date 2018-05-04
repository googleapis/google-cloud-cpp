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

# Run all the instance admin examples against production.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_instance_admin_examples() {
  local project_id=$1
  shift
  local zone_id=$1
  shift

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  local ignore_unimplemented="/bin/false"
  if [ ! -z "${BIGTABLE_EMULATOR_HOST:-}" ]; then
    ignore_unimplemented="echo Ignoring error as the emulator does not implement this function"
  fi

  echo
  echo "Run create-instance example."
  ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}" || ${ignore_unimplemented}

  echo
  echo "Run list-instances example."
  ../examples/bigtable_samples_instance_admin list-instances "${project_id}" || ${ignore_unimplemented}

  echo
  echo "Run get-instance example."
  ../examples/bigtable_samples_instance_admin get-instance "${project_id}" "${INSTANCE}" || ${ignore_unimplemented}

#  echo
#  echo "Run list-clusters example."
#  ../examples/bigtable_samples_instance_admin list-clusters "${project_id}" || ${ignore_unimplemented}

  echo
  echo "Run delete-instance example."
  ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}" || ${ignore_unimplemented}
}

# Run all the instance admin examples against production.
#
# This function allows us to keep a single place where all the examples are
# listed. We want to run these examples in the continuous integration builds
# because they rot otherwise.
function run_all_table_admin_examples() {
  local project_id=$1
  shift
  local zone_id=$1
  shift

  local ignore_unimplemented="/bin/false"
  if [ ! -z "${BIGTABLE_EMULATOR_HOST:-}" ]; then
    ignore_unimplemented="echo Ignoring error as the emulator does not implement this function"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Create an instance to run these examples.
  ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}" || ${ignore_unimplemented}

  echo
  echo "Run create-table example."
  ../examples/bigtable_samples create-table "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run list-tables example."
  ../examples/bigtable_samples list-tables "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run get-table example."
  ../examples/bigtable_samples get-table "${project_id}" "${INSTANCE}" sample-table

  # Populate some data on the table, so the next examples are meaningful.
  ../examples/bigtable_samples bulk-apply "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run modify-table example."
  ../examples/bigtable_samples modify-table "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run drop-rows-by-prefix example."
  ../examples/bigtable_samples drop-rows-by-prefix "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run drop-all-rows example."
  ../examples/bigtable_samples drop-all-rows "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run delete-table example."
  ../examples/bigtable_samples delete-table "${project_id}" "${INSTANCE}" sample-table

  # Delete the instance at the end examples.
  ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}" || ${ignore_unimplemented}
}

function run_all_data_examples {
  local project_id=$1
  shift
  local zone_id=$1
  shift

  local ignore_unimplemented="/bin/false"
  if [ ! -z "${BIGTABLE_EMULATOR_HOST:-}" ]; then
    ignore_unimplemented="echo Ignoring error as the emulator does not implement this function"
  fi

  # Create a (very likely unique) instance name.
  local -r INSTANCE="in-$(date +%s)"

  # Create an instance to run these examples.
  ../examples/bigtable_samples_instance_admin create-instance "${project_id}" "${INSTANCE}" "${zone_id}" || ${ignore_unimplemented}

  echo
  echo "Run create-table example."
  ../examples/bigtable_samples create-table "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run apply example."
  ../examples/bigtable_samples apply "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run bulk-apply example."
  ../examples/bigtable_samples bulk-apply "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run read-row example."
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run read-rows-with-limit example."
  ../examples/bigtable_samples read-rows-with-limit "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run scan/read-rows example."
  ../examples/bigtable_samples scan "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run sample-rows example."
  ../examples/bigtable_samples sample-rows "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run check-and-mutate example."
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples check-and-mutate "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table

  echo
  echo "Run read-modify-write example."
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-modify-write "${project_id}" "${INSTANCE}" sample-table
  ../examples/bigtable_samples read-row "${project_id}" "${INSTANCE}" sample-table

  # Delete the instance at the end examples.
  ../examples/bigtable_samples_instance_admin delete-instance "${project_id}" "${INSTANCE}" || ${ignore_unimplemented}
}
