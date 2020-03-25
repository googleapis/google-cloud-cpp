#!/usr/bin/env bash
#
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
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"
readonly BINDIR="$(dirname "$0")"
readonly GCSDIR="${PROJECT_ROOT}/google/cloud/storage"

# In the no-exceptions build this directory does not exist. Note that the script
# typically runs in ${CMAKE_PROJECT_BINARY_DIR}.
if [[ -d google/cloud/storage/examples ]]; then
  (cd google/cloud/storage/examples && \
      "${GCSDIR}/examples/run_examples_testbench.sh")
else
  echo "${COLOR_YELLOW}Skipping google/cloud/storage/examples.${COLOR_RESET}"
fi

if [[ -d google/cloud/storage/benchmarks ]]; then
  (cd google/cloud/storage/benchmarks && \
      "${GCSDIR}/benchmarks/run_benchmarks_testbench.sh")
else
  echo "${COLOR_YELLOW}Skipping google/cloud/storage/benchmarks.${COLOR_RESET}"
fi
