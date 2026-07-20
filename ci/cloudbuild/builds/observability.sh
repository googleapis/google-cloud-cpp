#!/bin/bash
#
# Copyright 2026 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
args+=(
  "--//google/cloud/bigtable:enable_metrics=true"
  "--dynamic_mode=off"
)

mapfile -t integration_args < <(integration::bazel_args)

observability_key_base="observability-key-$(date +"%Y-%m")"
readonly KEY_DIR="/dev/shm"
readonly SECRETS_BUCKET="gs://cloud-cpp-testing-resources-secrets"
gcloud storage cp --quiet "${SECRETS_BUCKET}/${observability_key_base}.json" "${KEY_DIR}/${observability_key_base}.json" >/dev/null 2>&1 || true
if [[ -r "${KEY_DIR}/${observability_key_base}.json" ]]; then
  GOOGLE_CLOUD_CPP_TEST_OBSERVABILITY_KEY_FILE_JSON="${KEY_DIR}/${observability_key_base}.json"
fi

ORIGINAL_ACCOUNT="$(gcloud config get-value account 2>/dev/null || true)"

# Activate dedicated observability service account credentials for GCE VM & GCS management
KEY_FILE=""
for candidate in \
  "${GOOGLE_CLOUD_CPP_TEST_OBSERVABILITY_KEY_FILE_JSON:-}" \
  "${GOOGLE_APPLICATION_CREDENTIALS:-}"; do
  if [[ -n "${candidate}" && -r "${candidate}" ]]; then
    KEY_FILE="${candidate}"
    break
  fi
done

if [[ -n "${KEY_FILE}" ]]; then
  io::log_h2 "Activating gcloud service account from ${KEY_FILE}"
  gcloud auth activate-service-account --key-file="${KEY_FILE}" --quiet || true
else
  io::log_yellow "No dedicated observability service account keyfile found; using default gcloud auth."
fi

io::log_h2 "Building OtelCollector targets and Bigtable Observability integration tests"
io::run bazel build "${args[@]}" \
  //ci/otel_collector:otel_collector_main \
  //google/cloud/bigtable/tests:observability_integration_test-default \
  //google/cloud/bigtable/tests:observability_integration_test-dynamic-pool

BAZEL_BIN="$(bazel info "${args[@]}" bazel-bin)"
TEST_DEFAULT_BIN="${BAZEL_BIN}/google/cloud/bigtable/tests/observability_integration_test-default"
TEST_DYNAMIC_BIN="${BAZEL_BIN}/google/cloud/bigtable/tests/observability_integration_test-dynamic-pool"

ZONE="us-central1-f"
PROJECT_ID="${GOOGLE_CLOUD_PROJECT:-cloud-cpp-testing-resources}"
BIGTABLE_INSTANCE_ID="${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID:-test-instance}"
BIGTABLE_CLUSTER_ID="${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID:-test-cluster}"
BIGTABLE_ZONE_A="${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A:-us-central1-f}"
BIGTABLE_ZONE_B="${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B:-us-central1-b}"
BUILD_SUFFIX="${BUILD_ID:-local}-${RANDOM}"
VM_NAME="dp-obs-test-${BUILD_SUFFIX:0:15}"
GCS_BUCKET="cloud-cpp-testing-resources-observability-logs"
GCS_PATH="gs://${GCS_BUCKET}/builds/${VM_NAME}"
VM_CREATED="false"

cleanup() {
  local exit_code=$?
  if [[ "${VM_CREATED:-false}" == "true" ]]; then
    io::log_h2 "Deleting ephemeral DirectPath VM: ${VM_NAME}"
    gcloud compute instances delete "${VM_NAME}" \
      --project="${PROJECT_ID}" \
      --zone="${ZONE}" \
      --quiet >/dev/null 2>&1 &
  fi

  if [[ -n "${ORIGINAL_ACCOUNT:-}" ]]; then
    io::log_h2 "Restoring original gcloud account: ${ORIGINAL_ACCOUNT}"
    gcloud config set account "${ORIGINAL_ACCOUNT}" --quiet >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT SIGINT SIGTERM

# Ensure GCS bucket and 90-day lifecycle policy exist
if ! gcloud storage buckets describe "gs://${GCS_BUCKET}" --project="${PROJECT_ID}" >/dev/null 2>&1; then
  io::log_h2 "Creating GCS bucket gs://${GCS_BUCKET} with 90-day lifecycle policy"
  gcloud storage buckets create "gs://${GCS_BUCKET}" \
    --project="${PROJECT_ID}" \
    --location="us-central1" \
    --uniform-bucket-level-access || true

  cat <<'EOF' >/tmp/observability_lifecycle.json
{
  "rule": [
    {
      "action": {
        "type": "Delete"
      },
      "condition": {
        "age": 90
      }
    }
  ]
}
EOF
  gcloud storage buckets update "gs://${GCS_BUCKET}" \
    --lifecycle-file=/tmp/observability_lifecycle.json \
    --project="${PROJECT_ID}" || true
fi

io::log_h2 "Uploading test binaries to GCS: ${GCS_PATH}/binaries/"
gcloud storage cp --quiet "${TEST_DEFAULT_BIN}" "${TEST_DYNAMIC_BIN}" "${GCS_PATH}/binaries/" >/dev/null 2>&1 || true

# Prepare Startup Script to run on the ephemeral GCE VM
STARTUP_SCRIPT=$(
  cat <<EOF
#!/bin/bash
exec > /tmp/startup.log 2>&1
set -x

export HOME="/tmp"
export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID="${BIGTABLE_INSTANCE_ID}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID="${BIGTABLE_CLUSTER_ID}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A="${BIGTABLE_ZONE_A}"
export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B="${BIGTABLE_ZONE_B}"
export CBT_ENABLE_DIRECTPATH=true

echo "Downloading test binaries from GCS..."
gcloud storage cp --quiet "${GCS_PATH}/binaries/*" /tmp/
chmod +x /tmp/observability_integration_test-default
chmod +x /tmp/observability_integration_test-dynamic-pool

TEST_EXIT_CODE=0

echo "Running observability_integration_test-default..."
/tmp/observability_integration_test-default \
  --gtest_output=xml:/tmp/test-default.xml > /tmp/test-default.log 2>&1 || TEST_EXIT_CODE=\$?

echo "Running observability_integration_test-dynamic-pool..."
/tmp/observability_integration_test-dynamic-pool \
  --gtest_output=xml:/tmp/test-dynamic-pool.xml > /tmp/test-dynamic-pool.log 2>&1 || TEST_EXIT_CODE=\$?

echo "\${TEST_EXIT_CODE}" > /tmp/exit_code.txt

echo "Uploading logs and test results back to GCS..."
gcloud storage cp --quiet /tmp/startup.log /tmp/*.log /tmp/*.xml /tmp/exit_code.txt "${GCS_PATH}/results/"
EOF
)

io::log_h2 "Creating ephemeral DirectPath VM instance: ${VM_NAME}"
if ! gcloud compute instances create "${VM_NAME}" \
  --project="${PROJECT_ID}" \
  --zone="${ZONE}" \
  --machine-type="e2-standard-4" \
  --subnet="projects/${PROJECT_ID}/regions/us-central1/subnetworks/directpath-subnet-ipv6" \
  --stack-type="IPV4_IPV6" \
  --image-family="ubuntu-2404-lts-amd64" \
  --image-project="ubuntu-os-cloud" \
  --metadata="startup-script=${STARTUP_SCRIPT}" \
  --scopes="cloud-platform" \
  --quiet >/tmp/vm_create.log 2>&1; then
  io::log_red "Failed to create ephemeral VM instance: ${VM_NAME}"
  cat /tmp/vm_create.log || true
  exit 1
fi
VM_CREATED="true"

io::log_h2 "Waiting for test execution to complete on VM ${VM_NAME}..."
TEST_COMPLETED="false"
for i in {1..90}; do
  if gcloud storage cp --quiet "${GCS_PATH}/results/exit_code.txt" /tmp/exit_code.txt >/dev/null 2>&1; then
    TEST_COMPLETED="true"
    io::log_yellow "Test execution on VM ${VM_NAME} finished."
    break
  fi
  sleep 5
done

if [[ "${TEST_COMPLETED}" != "true" ]]; then
  io::log_red "Timed out waiting for test execution on VM ${VM_NAME}."
  exit 1
fi

TEST_RESULT_CODE="$(cat /tmp/exit_code.txt 2>/dev/null || echo "1")"

io::log_h2 "Fetching test logs from GCS: ${GCS_PATH}/results/"
gcloud storage cp --quiet "${GCS_PATH}/results/*.log" /tmp/ 2>/dev/null || true

if [[ "${TEST_RESULT_CODE}" -ne 0 ]]; then
  for log_file in /tmp/startup.log /tmp/test-default.log /tmp/test-dynamic-pool.log; do
    if [[ -f "${log_file}" ]]; then
      io::log_h2 "=== Content of $(basename "${log_file}") ==="
      cat "${log_file}" || true
    fi
  done
  io::log_red "Observability integration tests failed with exit code: ${TEST_RESULT_CODE}"
  exit "${TEST_RESULT_CODE}"
else
  for log_file in /tmp/test-default.log /tmp/test-dynamic-pool.log; do
    if [[ -f "${log_file}" ]]; then
      io::log_h2 "=== Test Summary: $(basename "${log_file}" .log) (Execution: Live Ephemeral VM - Uncached) ==="
      grep -E '^\[(==========|----------| RUN      |       OK |  PASSED  |  FAILED  |  SKIPPED )\]' "${log_file}" || cat "${log_file}"
    fi
  done
  io::log_green "Observability integration tests PASSED successfully!"
fi
