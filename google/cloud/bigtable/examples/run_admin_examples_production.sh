#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/run_examples_utils.sh"
source "${BINDIR}/../tools/run_emulator_utils.sh"
source "${BINDIR}/../../../../ci/colors.sh"

# Run all the admin examples tests against production. Only nightly builds
# should run this script.
#
# The following environment variables must be defined before calling this
# script:
#
# - PROJECT_ID: the id (typically the string form) of a valid GCP project.
# - ZONE_A: the name of a valid GCP zone supporting Cloud Bigtable.
# - ZONE_B: the name of a valid GCP zone supporting Cloud Bigtable, should be
#   different from ZONE_A and should be in the same region.
# - INSTANCE_ID: the ID of an existing Cloud Bigtable instance in ZONE_A.
#

echo "${COLOR_YELLOW}run_quickstart_example${COLOR_RESET}"
run_quickstart_example "${PROJECT_ID}" "${INSTANCE_ID}"

echo "${COLOR_YELLOW}run_hello_instance_admin_example${COLOR_RESET}"
run_hello_instance_admin_example "${PROJECT_ID}" "${ZONE_A}"

echo "${COLOR_YELLOW}run_hello_table_admin_example${COLOR_RESET}"
run_hello_table_admin_example "${PROJECT_ID}" "${INSTANCE_ID}"

echo "${COLOR_YELLOW}run_hello_world_example${COLOR_RESET}"
run_hello_world_example "${PROJECT_ID}" "${INSTANCE_ID}"

echo "${COLOR_YELLOW}run_all_table_admin_examples${COLOR_RESET}"
run_all_table_admin_examples "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}"

echo "${COLOR_YELLOW}run_all_table_admin_async_examples${COLOR_RESET}"
run_all_table_admin_async_examples "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}"

echo "${COLOR_YELLOW}run_all_instance_admin_examples${COLOR_RESET}"
run_all_instance_admin_examples "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}"

echo "${COLOR_YELLOW}run_all_instance_admin_async_examples${COLOR_RESET}"
run_all_instance_admin_async_examples "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}"

exit_example_runner
