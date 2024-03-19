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

REVISION=$(sed -rn 's/  \"revision\": \"([[:digit:]]+)\",/\1/p' "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}")
readonly REVISION
io::log_h2 "git checkout -B update_compute_discovery_circa_${REVISION}"
git checkout -B "update_compute_discovery_circa_${REVISION}"

io::log_h2 "Adding updated Discovery JSON ${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"
git add "${PROJECT_ROOT}/${COMPUTE_DISCOVERY_JSON_RELATIVE_PATH}"

io::log_h2 "Running generate-libraries with UPDATED_DISCOVERY_DOCUMENT=compute"
UPDATED_DISCOVERY_DOCUMENT=compute ci/cloudbuild/build.sh -t generate-libraries-pr

if [[ -n "$(git ls-files --others --exclude-standard)" ]]; then
  io::log_red "New resources defined in Discovery Document created new protos:"
  git ls-files --others --exclude-standard
  io::log_red "Add rest_services definitions to the generator_config.textproto and re-run this script."
  git add protos
  exit 1
fi
