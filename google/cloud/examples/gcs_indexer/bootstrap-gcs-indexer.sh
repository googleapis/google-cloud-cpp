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
  echo "You must set GOOGLE_CLOUD_PROJECT to the project id hosting the GCS" \
      "metadata indexer"
  exit 1
fi

GOOGLE_CLOUD_PROJECT="${GOOGLE_CLOUD_PROJECT:-}"
readonly GOOGLE_CLOUD_PROJECT
GOOGLE_CLOUD_REGION="${REGION:-us-central1}"
readonly GOOGLE_CLOUD_REGION
GOOGLE_CLOUD_SPANNER_INSTANCE="${GOOGLE_CLOUD_SPANNER_INSTANCE:-gcs-index}"
readonly GOOGLE_CLOUD_SPANNER_INSTANCE
GOOGLE_CLOUD_SPANNER_DATABASE="${GOOGLE_CLOUD_SPANNER_DATABASE:-gcs-index-db}"
readonly GOOGLE_CLOUD_SPANNER_DATABASE

# Enable (if they are not enabled already) the services will we will need
gcloud services enable spanner.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable cloudbuild.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable containerregistry.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable run.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"
gcloud services enable pubsub.googleapis.com \
    "--project=${GOOGLE_CLOUD_PROJECT}"

# Create a Cloud Spanner instance to host the GCS metadata index
gcloud spanner instances create "${GOOGLE_CLOUD_SPANNER_INSTANCE}" \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--config=regional-${GOOGLE_CLOUD_REGION}" \
    --description="Store the GCS metadata index" \
    --nodes=3

# Build the Docker Images
gcloud builds submit \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--substitutions=SHORT_SHA=$(git rev-parse --short HEAD)" \
    "--config=cloudbuild.yaml"

# Create a service account for the Admin Deployment
gcloud iam service-accounts create gcs-index-admin \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  --description="Perform administrative updates to the GCS metadata index"

# Save the full name in a variable
ADMIN_SA="gcs-index-admin@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Gran ADMIN_SA permissions to create database and instances in the
gcloud spanner instances add-iam-policy-binding \
  "${GOOGLE_CLOUD_SPANNER_INSTANCE}" \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  "--member=serviceAccount:${ADMIN_SA}" \
  "--role=roles/spanner.databaseAdmin"
gcloud projects add-iam-policy-binding \
  "${GOOGLE_CLOUD_PROJECT}" \
  "--member=serviceAccount:${ADMIN_SA}" \
  "--role=roles/storage.objectViewer"

# Create the Admin Deployment
gcloud beta run deploy gcs-index-admin-handler \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  "--service-account=${ADMIN_SA}" \
  "--set-env-vars=SPANNER_PROJECT=${GOOGLE_CLOUD_PROJECT},SPANNER_INSTANCE=${GOOGLE_CLOUD_SPANNER_INSTANCE},SPANNER_DATABASE=${GOOGLE_CLOUD_SPANNER_DATABASE},GOOGLE_RUNNING_ON_GCE_CHECK_OVERRIDE=1" \
  "--image=gcr.io/${GOOGLE_CLOUD_PROJECT}/gcs-indexer-admin-handler:latest" \
  "--region=${GOOGLE_CLOUD_REGION}" \
  "--platform=managed" \
  "--no-allow-unauthenticated"

# Capture the service URL:
ADMIN_SERVICE_URL=$(gcloud beta run  services list \
    "--project=${GOOGLE_CLOUD_PROJECT}"\
    "--platform=managed" \
    '--format=csv[no-heading](URL)' \
    "--filter=SERVICE:gcs-index-admin-handler")

# Verify you have access to the service:
curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" \
  "${ADMIN_SERVICE_URL}"

# Post a request to create the database and table:
curl -X POST -H "Authorization: Bearer $(gcloud auth print-identity-token)" \
  "${ADMIN_SERVICE_URL}/create"

# To verify the database was created:
# gcloud spanner databases list
#     "--project=${GOOGLE_CLOUD_PROJECT}" \
#     "--instance=${GOOGLE_CLOUD_SPANNER_INSTANCE}"
#
# You should see something like this:
#
#    NAME       STATE
#    gcs-index  READY
#
# To verify the table was created:
#
# gcloud spanner databases ddl describe \
#     "--project=${GOOGLE_CLOUD_PROJECT}" \
#     "--instance=${GOOGLE_CLOUD_SPANNER_INSTANCE}" gcs-index
#
# You should see something like this:
#
#
#    --- |-
#      CREATE TABLE gcs_objects (
#        bucket STRING(128),
#        object STRING(1024),
#        generation STRING(128),
#        meta_generation STRING(128),
#        is_archived BOOL,
#        size INT64,
#        content_type STRING(256),
#        time_created TIMESTAMP,
#        updated TIMESTAMP,
#        storage_class STRING(256),
#        time_storage_class_updated TIMESTAMP,
#        md5_hash STRING(256),
#        crc32c STRING(256),
#        event_timestamp TIMESTAMP,
#      ) PRIMARY KEY(bucket, object, generation)
#

# Create a service account that will update the index

gcloud iam service-accounts create gcs-index-updater \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  --description="Perform updates to the GCS metadata index"

UPDATE_SA="gcs-index-updater@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Grant this service account permission to just update the table (but not create
# other tables or databases)
gcloud spanner instances add-iam-policy-binding \
  "${GOOGLE_CLOUD_SPANNER_INSTANCE}" \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  "--member=serviceAccount:${UPDATE_SA}" \
  "--role=roles/spanner.databaseUser"

# Create the Cloud Run deployment to update the index

gcloud beta run deploy gcs-index-pubsub-handler \
  "--project=${GOOGLE_CLOUD_PROJECT}" \
  "--service-account=${UPDATE_SA}" \
  --set-env-vars=SPANNER_PROJECT=${GOOGLE_CLOUD_PROJECT},SPANNER_INSTANCE=${GOOGLE_CLOUD_SPANNER_INSTANCE},SPANNER_DATABASE=${GOOGLE_CLOUD_SPANNER_DATABASE} \
  "--image=gcr.io/${GOOGLE_CLOUD_PROJECT}/gcs-indexer-pubsub-handler:latest" \
  "--region=us-central1" \
  "--platform=managed" \
  "--no-allow-unauthenticated"

# Capture the service URL:

SERVICE_URL=$(gcloud beta run services list \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--platform=managed" \
    '--format=csv[no-heading](URL)' \
    "--filter=SERVICE:pubsub-handler")

# Create the Cloud Pub/Sub topic and configuration

gcloud pubsub topics create gcs-index-updates \
  "--project=${GOOGLE_CLOUD_PROJECT}"

# Grant the Cloud Pub/Sub service account permissions to invoke Cloud Run

PROJECT_NUMBER=$(gcloud projects describe ${GOOGLE_CLOUD_PROJECT} \
    --format='csv[no-heading](projectNumber)')
gcloud projects add-iam-policy-binding ${GOOGLE_CLOUD_PROJECT} \
    "--member=serviceAccount:service-${PROJECT_NUMBER}@gcp-sa-pubsub.iam.gserviceaccount.com" \
    "--role=roles/iam.serviceAccountTokenCreator"
gcloud iam service-accounts create cloud-run-pubsub-invoker \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    --display-name "Cloud Run Pub/Sub Invoker"
gcloud beta run services add-iam-policy-binding gcs-index-pubsub-handler \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--region=us-central1" \
    "--platform=managed" \
    --member=serviceAccount:cloud-run-pubsub-invoker@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com \
    --role=roles/run.invoker

# Configure Cloud Pub/Sub to push updates to our Cloud Run Deployment

gcloud beta pubsub subscriptions create gcs-updates-run-subscription \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--topic=gcs-index-updates" \
    "--push-endpoint=${SERVICE_URL}/" \
    "--push-auth-service-account=cloud-run-pubsub-invoker@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

exit 0
