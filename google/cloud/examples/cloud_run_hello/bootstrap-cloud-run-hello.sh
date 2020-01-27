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

if [[ -z "${GOOGLE_CLOUD_PROJECT:-}" ]]; then
  echo "You must set GOOGLE_CLOUD_PROJECT to the project id hosting Cloud Run C++ Hello World"
  exit 1
fi

readonly GOOGLE_CLOUD_PROJECT="${GOOGLE_CLOUD_PROJECT:-}"
readonly GOOGLE_CLOUD_REGION="${REGION:-us-central1}"

# Enable (if they are not enabled already) the services will we will need
gcloud services enable cloudbuild.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable containerregistry.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable run.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"

# Build the Docker Images
gcloud builds submit \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--substitutions=SHORT_SHA=$(git rev-parse --short HEAD)" \
    "--config=cloudbuild.yaml"

# Create a service account that will update the index
readonly SA_ID="cloud-run-hello"
readonly SA_NAME="${SA_ID}@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
if gcloud iam service-accounts describe "${SA_NAME}" \
     "--project=${GOOGLE_CLOUD_PROJECT}" >/dev/null 2>&1; then
  echo "The ${SA_ID} service account already exists"
else
  gcloud iam service-accounts create "${SA_ID}" \
      "--project=${GOOGLE_CLOUD_PROJECT}" \
      --description="C++ Hello World for Cloud Run"
fi

# Create the Cloud Run deployment to update the index
gcloud beta run deploy cloud-run-hello \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--service-account=${SA_NAME}" \
    "--image=gcr.io/${GOOGLE_CLOUD_PROJECT}/cloud-run-hello:latest" \
    "--region=${GOOGLE_CLOUD_REGION}" \
    "--platform=managed" \
    "--no-allow-unauthenticated"

exit 0
