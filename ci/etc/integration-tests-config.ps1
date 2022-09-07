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

# The name of the project used to run the integration tests and examples.
$env:GOOGLE_CLOUD_PROJECT="cloud-cpp-testing-resources"
# Some quickstarts require a x-goog-user-project header, either when using
# our own user account in local builds, or when using the GCB service
# account
$env:GOOGLE_CLOUD_CPP_USER_PROJECT="${env:GOOGLE_CLOUD_PROJECT}"
# Many tests and quickstarts need a location, this is typically a region.
$env:GOOGLE_CLOUD_CPP_TEST_REGION="us-central1"
# Some quickstart programs require a zone.
$env:GOOGLE_CLOUD_CPP_TEST_ZONE="us-central1-a"

# Enable the self-test for the sample programs. Normally the example drivers
# require the name of the example to run as a command-line argument, with this
# environment variable the sample drivers run all the examples.
$env:GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES="yes"

# A number of options to improve logging during the CI builds. They are useful
# when troubleshooting problems.
$env:GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,1024,WARNING"
$env:GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc,rpc-streams"
$env:GOOGLE_CLOUD_CPP_TRACING_OPTIONS="single_line_mode=off,truncate_string_field_longer_than=512"
$env:CLOUD_STORAGE_ENABLE_TRACING="raw-client,rpc,rpc-streams"

# Cloud Bigtable configuration parameters
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="test-instance"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID="test-instance-c1"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="us-west2-b"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="us-west2-c"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT="bigtable-test-iam-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_QUICKSTART_TABLE="quickstart"

# Cloud Storage configuration parameters
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="gcs-grpc-team-cloud-cpp-testing-bucket"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME="gcs-grpc-team-cloud-cpp-testing-regional"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID="us-central1"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_LOCATION="${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT="storage-test-iam-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY="projects/${env:GOOGLE_CLOUD_PROJECT}/locations/us/keyRings/gcs-testing-us-kr/cryptoKeys/integration-tests-key"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME="projects/${env:GOOGLE_CLOUD_PROJECT}/topics/gcs-changes"
# We need a gzip file to test ReadObject() with decompressive transcoding
#  https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME="${env:PROJECT_ROOT}/ci/abi-dumps/google_cloud_cpp_storage.expected.abi.dump.gz"

# Cloud Spanner configuration parameters
$env:GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID="test-instance"
$env:GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT="spanner-iam-test-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_SPANNER_TEST_QUICKSTART_DATABASE="quickstart-db"
#$env:GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT="staging-wrenchworks.sandbox.googleapis.com"
#$env:GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_AUTHORITY="${env:GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT}"

# Cloud Pub/Sub configuration parameters
$env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC="quickstart"

# Cloud BigQuery configuration parameters
$env:GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE="projects/bigquery-public-data/datasets/usa_names/tables/usa_1910_current"

# Document AI
$env:GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_LOCATION_ID="us"
$env:GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_PROCESSOR_ID="3cb572567f9df97f"
$env:GOOGLE_CLOUD_CPP_DOCUMENTAI_TEST_FILENAME="${env:PROJECT_ROOT}/google/cloud/documentai/quickstart/resources/invoice.pdf"

# Cloud IAM configuration parameters
$env:GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT="iam-credentials-test-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT="iam-test-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT="invalid-test-account@cloud-cpp-testing-resources.iam.gserviceaccount.com"

# Rest configuration parameters
$env:GOOGLE_CLOUD_CPP_REST_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# To run google/cloud/gameservices' quickstart
$env:GOOGLE_CLOUD_CPP_GAMESERVICES_TEST_LOCATION="global"
$env:GOOGLE_CLOUD_CPP_GAMESERVICES_TEST_REALM="test-realm"

# To run google/cloud/resourcemanager's quickstart, this is the Cloud C++ team community folder
$env:GOOGLE_CLOUD_CPP_RESOURCEMANAGER_TEST_FOLDER="204009073908"
