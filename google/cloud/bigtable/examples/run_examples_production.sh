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

readonly BINDIR="$(dirname "$0")"
source "${BINDIR}/run_examples_utils.sh"

# In the presubmit and continuous integration builds we only run the data
# examples against production. The CI systems go over project quotas for admin
# operations if we run the admin examples too.
echo "${COLOR_YELLOW}run_all_data_examples${COLOR_RESET}"
run_all_data_examples "${PROJECT_ID}" "${INSTANCE_ID}"

echo "${COLOR_YELLOW}run_all_data_async_examples${COLOR_RESET}"
run_all_data_async_examples "${PROJECT_ID}" "${INSTANCE_ID}"

exit_example_runner
