#!/bin/bash
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This script manages Google Cloud Build scheduled builds. The schedules are
# defined by JSON files that live in the `schedules/` directory. To create one
# of these files for an existing schedule use --export. These files can be
# used to create and update schedules (--import).
#
# Usage: schedule.sh [options]
#
#   Options:
#     --list              List all schedules for this project
#     --ids               List the IDSs of all schedules this project
#     --export            Exports an existing schedule
#     --import            Creates or updates a schedule
#     -h|--help           Print this help message

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

readonly CLOUD_PROJECT="cloud-cpp-testing-resources"
readonly LOCATION=us-central1

function list_schedules() {
  gcloud scheduler jobs list \
    --project "${CLOUD_PROJECT}" \
    --location "${LOCATION}"
}

function ids_schedules() {
  gcloud scheduler jobs list \
    --project "${CLOUD_PROJECT}" \
    --location "${LOCATION}" \
    --format "value(ID)"
}

function export_schedule() {
  local name="$1"
  gcloud scheduler jobs describe "${name}" \
    --project "${CLOUD_PROJECT}" \
    --location "${LOCATION}" \
    --format=json
}

function schedule_flags() {
  local file="$1"
  # jq requires a single argument, but it is too long.
  local args=(
    '"--schedule"' .schedule
    '"--uri"' .httpTarget.uri
    '"--oauth-service-account-email"' .httpTarget.oauthToken.serviceAccountEmail
    '"--oauth2-service-account-scope"' .httpTarget.oauthToken.scope
  )
  local script
  script="$(print ", %s" "${args[@]}")"
  jq -r "${script:2}" <"${file}"
}

function import_schedule() {
  local file="$1"
  local id
  id="$(basename "${file}" .json)"
  local -a flags
  mapfile -t flags < <(schedule_flags "${file}")

  io::run gcloud scheduler jobs create http "${id}" "${flags[@]}" \
    --project "${CLOUD_PROJECT}" \
    --location "${LOCATION}"
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="h" \
  --longoptions="list,ids,export:,import:,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

case "$1" in
  --list)
    list_schedules
    ;;
  --ids)
    ids_schedules
    ;;
  --export)
    export_schedule "$2"
    ;;
  --import)
    import_schedule "$2"
    ;;
  -h | --help)
    print_usage
    ;;
  *)
    print_usage
    exit 1
    ;;
esac
