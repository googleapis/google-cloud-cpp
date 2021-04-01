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

# Make our include guard clean against set -o nounset.
test -n "${CI_ETC_INTEGRATION_TESTS_CONFIG_SH__:-}" || declare -i CI_ETC_INTEGRATION_TESTS_CONFIG_SH__=0
if ((CI_ETC_INTEGRATION_TESTS_CONFIG_SH__++ != 0)); then
  return 0
fi # include guard

#
# Common configuration parameters.
#
export GOOGLE_CLOUD_PROJECT="cloud-cpp-testing-resources"

# Cloud Bigtable configuration parameters
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="test-instance"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID="test-instance-c1"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="us-west2-b"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="us-west2-c"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT="bigtable-test-iam-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Cloud Storage configuration parameters
# An existing bucket, used in small tests that do not change the bucket metadata.
export GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="cloud-cpp-testing-bucket"
# A bucket with a different location and/or storage class from
# `cloud-cpp-testing-bucket`, some requests (object copy and rewrite) succeed
# immediately with buckets in the same location, and we want to demonstrate we
# can handle partial success.
export GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME="cloud-cpp-testing-regional"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID="us-central1"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_LOCATION="${GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT="storage-test-iam-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="kokoro-run@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY="projects/${GOOGLE_CLOUD_PROJECT}/locations/us/keyRings/gcs-testing-us-kr/cryptoKeys/integration-tests-key"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME="projects/${GOOGLE_CLOUD_PROJECT}/topics/gcs-changes"

# Cloud Spanner configuration parameters
export GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID="test-instance"
export GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT="spanner-iam-test-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"

# Cloud Pub/Sub configuration parameters
export GOOGLE_CLOUD_CPP_PUBSUB_TEST_QUICKSTART_TOPIC="quickstart"

# Cloud BigQuery configuration parameters
export GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE="projects/bigquery-public-data/datasets/usa_names/tables/usa_1910_current"

# Cloud IAM configuration parameters
export GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}
export GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT="invalid-test-account@cloud-cpp-testing-resources.iam.gserviceaccount.com"
