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
  service_proto_path="${1#protos/}"
  product_path="${service_proto_path%/*.proto}"
  initial_copyright_year=$(date +"%Y")

  (
    sed -n '/# update_discovery_doc.sh additions/q;p' "${PROJECT_ROOT}/${GENERATOR_CONFIG_RELATIVE_PATH}"
    cat <<_EOF_
  rest_services {
    service_proto_path: "${service_proto_path}"
    product_path: "${product_path}"
    initial_copyright_year: "${initial_copyright_year}"
    retryable_status_codes: ["kUnavailable"]
    generate_rest_transport: true
    generate_grpc_transport: false
  }

_EOF_
    sed -n '/# update_discovery_doc.sh additions/,$p' "${PROJECT_ROOT}/${GENERATOR_CONFIG_RELATIVE_PATH}"
  ) | sponge "${PROJECT_ROOT}/${GENERATOR_CONFIG_RELATIVE_PATH}"
}

function add_service_directory() {
  compute_proto_path="${1#protos/google/cloud/compute/}"
  service_dir="${compute_proto_path%/*.proto}/"
  echo "    \"${service_dir}\""
  sed -i -f - "${PROJECT_ROOT}/${COMPUTE_SERVICE_DIRS_CMAKE_RELATIVE_PATH}" <<EOT
  /^set(service_dirs$/ {
    n  # skip "cmake-format: sort" line
    a\    "${service_dir}"
  }
EOT
}

CREATE_BRANCH=true
while (($# > 0)); do
  case "$1" in
    --create-branch=true)
      CREATE_BRANCH=true
      shift
      ;;
    --create-branch=false | --no-create-branch)
      CREATE_BRANCH=false
      shift
      ;;
    *)
      cat 1>&2 <<EOM
Usage: $(basename "$0") [--create-branch=true|false]

  Updates the Compute Discovery Document and runs the generator on the updated
  JSON.

  --create-branch=true|false (default: true)
      Whether to create and check out a new git branch update_compute_discovery_circa_<REVISION>.
EOM
      exit 1
      ;;
  esac
done
readonly CREATE_BRANCH

readonly COMPUTE_DISCOVERY_DOCUMENT_URL="https://raw.githubusercontent.com/googleapis/discovery-artifact-manager/master/discoveries/compute.v1.json"
readonly COMPUTE_DISCOVERY_JSON_RELATIVE_PATH="generator/discovery/compute_public_google_rest_v1.json"
readonly GENERATOR_CONFIG_RELATIVE_PATH="generator/generator_config.textproto"
readonly COMPUTE_SERVICE_DIRS_CMAKE_RELATIVE_PATH="google/cloud/compute/service_dirs.cmake"
readonly COMPUTE_SERVICE_DIRS_BZL_RELATIVE_PATH="google/cloud/compute/service_dirs.bzl"

CURRENT_REVISION=$(sed -En 's/  \"revision\": \"([[:digit:]]+)\",/\1/p' "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}" 2>/dev/null || echo "")

io::log_h2 "Fetching discovery document from ${COMPUTE_DISCOVERY_DOCUMENT_URL}"
TEMP_JSON=$(mktemp)
trap 'rm -f "${TEMP_JSON}"' EXIT
curl -fsSL "${COMPUTE_DISCOVERY_DOCUMENT_URL}" >"${TEMP_JSON}"

REVISION=$(sed -En 's/  \"revision\": \"([[:digit:]]+)\",/\1/p' "${TEMP_JSON}")
if [[ -z "${REVISION}" ]]; then
  io::log_red "Could not parse revision from fetched discovery document."
  exit 1
fi
readonly REVISION

if [[ -n "${CURRENT_REVISION}" && "${REVISION}" == "${CURRENT_REVISION}" ]]; then
  io::log_green "Compute discovery document is already up to date (revision ${CURRENT_REVISION}). Nothing to do."
  exit 0
fi

io::log_h2 "Updating Discovery Document: ${CURRENT_REVISION:-none} -> ${REVISION}"
mv "${TEMP_JSON}" "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"

if [[ "${CREATE_BRANCH}" == "true" ]]; then
  io::run git checkout -B update_compute_discovery_circa_"${REVISION}"
fi

io::log_h2 "Adding updated Discovery JSON ${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"
git commit -m"chore(compute): update discovery doc circa ${REVISION}" \
  "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"

io::log_h2 "Running generate-libraries with UPDATED_DISCOVERY_DOCUMENT=compute"
UPDATED_DISCOVERY_DOCUMENT=compute ci/cloudbuild/build.sh -t generate-libraries-pr

io::log_h2 "Adding new compute internal protos"
git add "${PROJECT_ROOT}/protos/google/cloud/compute/v1/internal"

NEW_FILES=$(git ls-files --others --exclude-standard protos/)
if [[ -n "${NEW_FILES}" ]]; then
  git add protos/google/cloud/compute

  io::log_h2 "Formatting generated protos"
  git ls-files -z -- '*.proto' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
  git ls-files -z -- '*.proto' |
    xargs -r -P "$(nproc)" -n 50 -0 sed -i 's/[[:blank:]]\+$//'

  io::log_red "New resources defined in Discovery Document created new protos:"
  echo "${NEW_FILES}"
  mapfile -t proto_array < <(echo "${NEW_FILES}")
  io::log_red "Adding rest_services definitions to the generator_config.textproto."
  for i in "${proto_array[@]}"; do
    echo "${i}"
    print_service_textproto "${i}"
  done
fi

io::log_h2 "Running generate-libraries with updated generator_config.textproto"
ci/cloudbuild/build.sh -t generate-libraries-pr || true

if [[ -n "${NEW_FILES}" ]]; then
  io::log_h2 "Adding new compute generated service code"
  git add google/cloud/compute

  io::log_h2 "Formatting generated code"
  git ls-files -z -- '*.h' '*.cc' |
    xargs -P "$(nproc)" -n 50 -0 clang-format -i
  git ls-files -z -- '*.h' '*.cc' |
    xargs -r -P "$(nproc)" -n 50 -0 sed -i 's/[[:blank:]]\+$//'

  io::log_yellow "Adding new directories to ${COMPUTE_SERVICE_DIRS_CMAKE_RELATIVE_PATH}"
  for i in "${proto_array[@]}"; do
    add_service_directory "${i}"
  done

  io::log_yellow "Adding new directories to ${COMPUTE_SERVICE_DIRS_BZL_RELATIVE_PATH}"
  CMAKE_BUILD_DIR=$(mktemp -d)
  cmake -DGOOGLE_CLOUD_CPP_ENABLE=compute -S . -B "${CMAKE_BUILD_DIR}"

  git commit -m"Update generator_config.textproto and service_dirs files" \
    "${PROJECT_ROOT}/${GENERATOR_CONFIG_RELATIVE_PATH}" \
    "${PROJECT_ROOT}/${COMPUTE_SERVICE_DIRS_CMAKE_RELATIVE_PATH}" \
    "${PROJECT_ROOT}/${COMPUTE_SERVICE_DIRS_BZL_RELATIVE_PATH}"
fi

git add -A . && git commit -m"Update generated code"
