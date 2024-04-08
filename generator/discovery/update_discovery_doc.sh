#!/usr/bin/env bash
#
# Copyright 2024 Google LLC
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

set -euo pipefail

source "$(dirname "$0")/../../ci/lib/init.sh"
source module ci/lib/io.sh

function print_service_textproto() {
  service_proto_path=$(echo "${1}" | sed -En 's/protos\/(google\/cloud\/compute\/.*\/v[[:digit:]]\/.*\.proto)/\1/p')
  product_path=$(echo "${1}" | sed -En 's/protos\/(google\/cloud\/compute\/.*\/v[[:digit:]])\/.*\.proto/\1/p')
  initial_copyright_year=$(date +"%Y")
  echo "  rest_services {"
  echo "    service_proto_path: \"${service_proto_path}\""
  echo "    product_path: \"${product_path}\""
  echo "    initial_copyright_year: \"${initial_copyright_year}\""
  echo "    retryable_status_codes: [\"kUnavailable\"]"
  echo "    generate_rest_transport: true"
  echo "    generate_grpc_transport: false"
  echo "  }"
}

function add_service_directory() {
  service_dir=$(echo "${1}" | sed -En 's/protos\/google\/cloud\/compute\/(.*\/v[[:digit:]]\/).*\.proto/\1/p')
  echo "    \"${service_dir}\""
  sed -i "19i\    \"${service_dir}\"" "${PROJECT_ROOT}/google/cloud/compute/service_dirs.cmake"
}

if (($# > 0)); then
  cat 1>&2 <<EOM
Usage: $(basename "$0")

  Updates the Compute Discovery Document and runs the generator on the updated
  JSON.
EOM
  exit 1
fi

readonly COMPUTE_DISCOVERY_DOCUMENT_URL="https://www.googleapis.com/discovery/v1/apis/compute/v1/rest"
readonly COMPUTE_DISCOVERY_JSON_RELATIVE_PATH="generator/discovery/compute_public_google_rest_v1.json"

io::log_h2 "Fetching discovery document from ${COMPUTE_DISCOVERY_DOCUMENT_URL}"
curl "${COMPUTE_DISCOVERY_DOCUMENT_URL}" >"${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"

REVISION=$(sed -En 's/  \"revision\": \"([[:digit:]]+)\",/\1/p' "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}")
readonly REVISION
io::run git checkout -B update_compute_discovery_circa_"${REVISION}"

io::log_h2 "Adding updated Discovery JSON ${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"
git add "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"

io::log_h2 "Running generate-libraries with UPDATED_DISCOVERY_DOCUMENT=compute"
UPDATED_DISCOVERY_DOCUMENT=compute ci/cloudbuild/build.sh -t generate-libraries-pr

NEW_FILES=$(git ls-files --others --exclude-standard)
if [[ -n "${NEW_FILES}" ]]; then
  io::log_red "New resources defined in Discovery Document created new protos:"
  echo "${NEW_FILES}"
  mapfile -t proto_array < <(echo "${NEW_FILES}")
  io::log_red "Add rest_services definitions to the generator_config.textproto and re-run this script."
  for i in "${proto_array[@]}"; do
    print_service_textproto "${i}"
  done
  git add protos
  io::log_yellow "Adding new directories to google/cloud/compute/service_dirs.cmake"
  for i in "${proto_array[@]}"; do
    add_service_directory "${i}"
  done
  exit 1
fi
