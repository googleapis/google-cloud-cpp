#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

if [[ $# -lt 3 ]]; then
  echo "Usage $0 <project-id> <instance-id> <database-id>"
  exit 1
fi

readonly PROJECT_ID="${1}"
readonly SPANNER_INSTANCE="${2}"
readonly SPANNER_DATABASE="${3}"

# Pick a name for the service account, for example:
readonly SA_NAME=${SA_NAME:-generate-objects-sa}
readonly SA_NAME_FULL="${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"

# Then create the service account:
if gcloud iam service-accounts describe ${SA_NAME_FULL}; then
  echo "The ${SA_NAME} service account already exists"
else
  gcloud iam service-accounts create "${SA_NAME}" \
      "--project=${PROJECT_ID}" \
      "--display-name=Service account to index GCS data"
fi

# Grant the service account `roles/spanner.databaseUser` permissions on the database
gcloud spanner instances add-iam-policy-binding \
  "${SPANNER_INSTANCE}" \
  "--project=${PROJECT_ID}" \
  "--member=serviceAccount:${SA_NAME_FULL}" \
  "--role=roles/spanner.databaseUser"
