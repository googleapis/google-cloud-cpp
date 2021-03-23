#!/bin/bash
#
# Copyright 2021 Google LLC
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
#
# This script manages Google Cloud Build triggers. The triggers are defined by
# yaml files that live in the `triggers/` directory.
#
# Usage: ./trigger.sh [options]
#
#   Options:
#     -l|--list           List all triggers for this repo on the server
#     -d|--describe=name  Describe the named trigger
#     -i|--import=file    Uploads the local yaml file to create/update a trigger
#     -h|--help           Print this help message

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

readonly CLOUD_PROJECT="cloud-cpp-testing-resources"
readonly GITHUB_NAME="google-cloud-cpp"

function list_triggers() {
  gcloud beta builds triggers list \
    --project "${CLOUD_PROJECT}" \
    --filter "github.name = ${GITHUB_NAME}"
}

function describe_trigger() {
  local name="$1"
  gcloud beta builds triggers describe "${name}" \
    --project "${CLOUD_PROJECT}"
}

function import_trigger() {
  local file="$1"
  gcloud beta builds triggers import \
    --source "${file}" --project "${CLOUD_PROJECT}" | tee "${file}.IMPORTED"
  mv "${file}.IMPORTED" "${file}"
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="d:i:lh" \
  --longoptions="describe:,import:,list,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

case "$1" in
-d | --describe)
  describe_trigger "$2"
  ;;
-i | --import)
  import_trigger "$2"
  ;;
-l | --list)
  list_triggers
  ;;
-h | --help)
  print_usage
  ;;
*)
  print_usage
  ;;
esac
