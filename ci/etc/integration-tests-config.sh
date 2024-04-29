#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
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

# Make our include guard clean against set -o nounset.
test -n "${CI_ETC_INTEGRATION_TESTS_CONFIG_SH__:-}" || declare -i CI_ETC_INTEGRATION_TESTS_CONFIG_SH__=0
if ((CI_ETC_INTEGRATION_TESTS_CONFIG_SH__++ != 0)); then
  return 0
fi # include guard

# The name of the project used to run the integration tests and examples.
export GOOGLE_CLOUD_PROJECT="cloud-cpp-testing-resources"
# Some services seemingly require project numbers. If you ever need to generate
# this again use:
#   gcloud projects describe "${GOOGLE_CLOUD_PROJECT}"
export GOOGLE_CLOUD_CPP_TEST_PROJECT_NUMBER="936212892354"
# Some quickstarts require a x-goog-user-project header, either when using
# our own user account in local builds, or when using the GCB service
# account
export GOOGLE_CLOUD_CPP_USER_PROJECT="${GOOGLE_CLOUD_PROJECT}"
# Many tests and quickstarts need a location, this is typically a region.
export GOOGLE_CLOUD_CPP_TEST_REGION="us-central1"
# Some quickstart programs require a zone.
export GOOGLE_CLOUD_CPP_TEST_ZONE="us-central1-a"
# Some quickstart programs require an organization.
export GOOGLE_CLOUD_CPP_TEST_ORGANIZATION="1006341795026"

# This file contains an invalidated service account key.  That is, the file is
# in the right format for a service account, but it is not associated with a
# valid service account or service account key.
export GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE="${PROJECT_ROOT}/ci/etc/invalidated-keyfile.json"

# Enable the self-test for the sample programs. Normally the example drivers
# require the name of the example to run as a command-line argument, with this
# environment variable the sample drivers run all the examples.
export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES="yes"

# A number of options to improve logging during the CI builds. They are useful
# when troubleshooting problems.
export GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,1024,WARNING"
export GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc,rpc-streams"
export GOOGLE_CLOUD_CPP_TRACING_OPTIONS="single_line_mode=off,truncate_string_field_longer_than=512"
export CLOUD_STORAGE_ENABLE_TRACING="raw-client,rpc,rpc-streams"

# Cloud Bigtable configuration parameters
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="test-instance"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID="test-instance-c1"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="us-central1-b"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="us-central1-c"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT="bigtable-test-iam-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_QUICKSTART_TABLE="quickstart"

# Cloud Storage configuration parameters
# An existing bucket, used in small tests that do not change the bucket metadata.
export GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="gcs-grpc-team-cloud-cpp-testing-bucket"
# Another preexisting bucket, created with folders enabled, for tests that do
# not modify the bucket:
export GOOGLE_CLOUD_CPP_STORAGE_TEST_FOLDER_BUCKET_NAME="cloud-cpp-testing-folder-bucket"
# A bucket with a different location and/or storage class from
# `cloud-cpp-testing-bucket`, some requests (object copy and rewrite) succeed
# immediately with buckets in the same location, and we want to demonstrate we
# can handle partial success.
export GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME="gcs-grpc-team-cloud-cpp-testing-regional"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID="us-central1"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_LOCATION="${GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT="storage-test-iam-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY="projects/${GOOGLE_CLOUD_PROJECT}/locations/us/keyRings/gcs-testing-us-kr/cryptoKeys/integration-tests-key"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME="projects/${GOOGLE_CLOUD_PROJECT}/topics/gcs-changes"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT="fake-service-account-hmac@example.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"
# We need a gzip file to test ReadObject() with decompressive transcoding
#  https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
export GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME="${PROJECT_ROOT}/ci/data/fox.txt.gz"

# Cloud Spanner configuration parameters
export GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID="test-instance"
export GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT="spanner-iam-test-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_SPANNER_TEST_QUICKSTART_DATABASE="quickstart-db"
#export GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT="staging-wrenchworks.sandbox.googleapis.com"
#export GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_AUTHORITY="${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT}"

# Cloud Pub/Sub configuration parameters
export GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC="quickstart"

# Cloud Batch configuration parameters
# Created using:
#    gcloud compute instance-templates create \
#        --project=cloud-cpp-testing-resources "cloud-batch-sample-template" \
#        --region us-central1 --machine-type e2-standard-4
export GOOGLE_CLOUD_CPP_BATCH_TEST_TEMPLATE_NAME="cloud-batch-sample-template"

# Cloud BigQuery configuration parameters
export GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE="projects/bigquery-public-data/datasets/usa_names/tables/usa_1910_current"

# Content Warehouse
export GOOGLE_CLOUD_CPP_CONTENTWAREHOUSE_TEST_LOCATION_ID="us"

# Document AI
export GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_LOCATION_ID="us"
export GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_PROCESSOR_ID="3cb572567f9df97f"
export GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_FILENAME="${PROJECT_ROOT}/google/cloud/documentai/quickstart/resources/invoice.pdf"

# Cloud IAM configuration parameters
export GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT="iam-credentials-test-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT="iam-test-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT="invalid-test-account@cloud-cpp-testing-resources.iam.gserviceaccount.com"

# Cloud IAM credential samples configuration parameters
# An account able to invoke the Hello World service(s)
export GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_SERVICE_ACCOUNT="hello-world-caller@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
# The URL for the HTTP Hello World service, typically set by the CI scripts
export GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_HTTP_URL=""

# Rest configuration parameters
export GOOGLE_CLOUD_CPP_REST_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# To run google/cloud/resourcemanager's quickstart, this is the Cloud C++ team community folder
export GOOGLE_CLOUD_CPP_RESOURCEMANAGER_TEST_FOLDER="204009073908"

# To run google/cloud/gkemulticloud's quickstart. The service is not available in `us-central1`
export GOOGLE_CLOUD_CPP_GKEMULTICLOUD_TEST_REGION="us-west1"

# google/cloud/policysimulator's quickstart
export GOOGLE_CLOUD_CPP_POLICYSIMULATOR_RESOURCE="//cloudresourcemanager.googleapis.com/projects/cloud-cpp-community"

# google/cloud/policytroubleshooter's quickstart
export GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_PRINCIPAL="${GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT}"
export GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_RESOURCE="//cloudresourcemanager.googleapis.com/projects/${GOOGLE_CLOUD_PROJECT}"
export GOOGLE_CLOUD_CPP_POLICYTROUBLESHOOTER_PERMISSION="storage.buckets.get"

# To run google/cloud/commerce's quickstart
# This is a closed billing account for testing.
export GOOGLE_CLOUD_CPP_COMMERCE_TEST_BILLING_ACCOUNT="01B3AC-DE5ABE-5A1ED5"
