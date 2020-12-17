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

#
# Common configuration parameters.
#
$env:GOOGLE_CLOUD_PROJECT="cloud-cpp-testing-resources"

# Cloud Bigtable configuration parameters
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="test-instance"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID="test-instance-c1"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="us-west2-b"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="us-west2-c"
$env:GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT="bigtable-test-iam-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Cloud Storage configuration parameters
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="cloud-cpp-testing-bucket"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME="cloud-cpp-testing-regional"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID="us-central1"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_LOCATION="${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT="storage-test-iam-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY="projects/${env:GOOGLE_CLOUD_PROJECT}/locations/us/keyRings/gcs-testing-us-kr/cryptoKeys/integration-tests-key"
$env:GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME="projects/${env:GOOGLE_CLOUD_PROJECT}/topics/gcs-changes"

# Cloud Spanner configuration parameters
$env:GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID="test-instance"
$env:GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT="spanner-iam-test-sa@${env:GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Cloud Pub/Sub only needs GOOGLE_CLOUD_PROJECT
$env:GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC="quickstart"

# Cloud IAM configuration parameters
$env:GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${env:GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}
$env:GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT="invalid-test-account@cloud-cpp-testing-resources.iam.gserviceaccount.com"
