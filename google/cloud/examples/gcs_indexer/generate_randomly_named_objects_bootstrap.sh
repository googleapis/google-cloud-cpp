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
  echo "Usage $0 <project-id> <bucket-name> <region>"
  exit 1
fi

PROJECT_ID="${1}"
readonly PROJECT_ID
BUCKET_NAME="${2}"
readonly BUCKET_NAME
REGION="${3}"
readonly REGION

# Create the GKE cluster
if gcloud container clusters describe gcs-indexing \
    "--project=${PROJECT_ID}" \
    "--region=${REGION}" >/dev/null 2>&1; then
  echo "Cluster gcs-indexing already exists, reusing"
else
  gcloud container clusters create gcs-indexing \
      "--project=${PROJECT_ID}" \
      "--region=${REGION}" \
      "--preemptible"
    gcloud container clusters update gcs-indexing  \
      "--project=${PROJECT_ID}" \
      "--region=${REGION}" \
      "--enable-autoscaling" \
      "--min-nodes=1" "--max-nodes=20" \
      "--node-pool=default-pool"
fi

gcloud container clusters get-credentials gcs-indexing \
    "--project=${PROJECT_ID}" \
    "--region=${REGION}"

# Pick a name for the service account, for example:
SA_NAME=${SA_NAME:-generate-objects-sa}
readonly SA_NAME
SA_NAME_FULL="${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"
readonly SA_NAME_FULL

# Then create the service account:
if gcloud iam service-accounts describe ${SA_NAME_FULL}; then
  echo "The ${SA_NAME} service account already exists"
else
  gcloud iam service-accounts create "${SA_NAME}" \
      "--project=${PROJECT_ID}" \
      "--display-name=Service account to run GCS C++ Client Benchmarks"

  # Create new keys for this service account and download then to a temporary
  # place:
  gcloud iam service-accounts keys create "/dev/shm/key.json" \
      "--iam-account=${SA_NAME_FULL}"

  # Copy the key to the GKE cluster:
  kubectl create secret generic service-account-key \
        "--from-file=key.json=/dev/shm/key.json"

  rm "/dev/shm/key.json"
fi

# Grant the service account `roles/storage.admin` permissions on the bucket
gsutil iam ch \
    "serviceAccount:${SA_NAME_FULL}:roles/storage.admin" \
    "gs://${BUCKET_NAME}"
