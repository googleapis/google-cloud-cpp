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
source module ci/cloudbuild/builds/lib/git.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/etc/integration-tests-config.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

PROJECT_ID="${GOOGLE_CLOUD_PROJECT:-cloud-cpp-testing-resources}"
REGION="${GOOGLE_CLOUD_CPP_TEST_REGION:-us-central1}"
# integration-tests-config.sh exports GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME (without UBLA).
# Agent Identity bearer tokens require Uniform Bucket-Level Access (UBLA).
BUCKET_NAME="cloud-cpp-wif-test-bucket"

# Ensure gcloud is updated and alpha component is available in the runner
io::log_h2 "Updating gcloud and installing alpha components"
gcloud --quiet components update || true
gcloud --quiet components install alpha || true

BUILD_SUFFIX="${BUILD_ID:-local}-${RANDOM}"
JOB_NAME="mwlid-job-${BUILD_SUFFIX:0:15}"
IMAGE_TAG="gcr.io/${PROJECT_ID}/managed-workload-test:${JOB_NAME}"
JOB_CREATED="false"

cleanup() {
  local exit_code=$?
  if [[ "${JOB_CREATED}" == "true" ]]; then
    io::log_h2 "Cloud Run Job logs for ${JOB_NAME}:"
    gcloud logging read "resource.type=cloud_run_job AND resource.labels.job_name=${JOB_NAME}" \
      --project="${PROJECT_ID}" \
      --limit=100 \
      --format="value(textPayload)" 2>/dev/null || true

    io::log_h2 "Cleaning up ephemeral Cloud Run Job: ${JOB_NAME}"
    gcloud run jobs delete "${JOB_NAME}" \
      --project="${PROJECT_ID}" \
      --region="${REGION}" \
      --quiet >/dev/null 2>&1 || true
  fi

  io::log_h2 "Cleaning up container image: ${IMAGE_TAG}"
  gcloud container images delete "${IMAGE_TAG}" \
    --quiet --force-delete-tags >/dev/null 2>&1 || true

  exit "${exit_code}"
}
trap cleanup EXIT SIGINT SIGTERM

io::log_h2 "Building managed_workload_integration_test with Bazel"
mapfile -t args < <(bazel::common_args)
args+=("--dynamic_mode=off")
io::run bazel build "${args[@]}" //google/cloud/storage/tests:managed_workload_integration_test

BAZEL_BIN="$(bazel info "${args[@]}" bazel-bin)"
TEST_BIN="${BAZEL_BIN}/google/cloud/storage/tests/managed_workload_integration_test"

io::log_h2 "Staging container build artifacts"
DOCKERFILE_DIR="$(mktemp -d)"
cp "${TEST_BIN}" "${DOCKERFILE_DIR}/managed_workload_test"
cat <<'EOF' >"${DOCKERFILE_DIR}/Dockerfile"
FROM ubuntu:24.04
RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates libssl3 libcurl4 && \
    rm -rf /var/lib/apt/lists/*
COPY managed_workload_test /usr/local/bin/managed_workload_test
RUN chmod +x /usr/local/bin/managed_workload_test
ENTRYPOINT ["/usr/local/bin/managed_workload_test"]
EOF

io::log_h2 "Building and pushing container image: ${IMAGE_TAG}"
if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  docker build -t "${IMAGE_TAG}" "${DOCKERFILE_DIR}"
  docker push "${IMAGE_TAG}"
else
  gcloud builds submit --tag "${IMAGE_TAG}" "${DOCKERFILE_DIR}" --project="${PROJECT_ID}" --quiet
fi
rm -rf "${DOCKERFILE_DIR}"

io::log_h2 "Deploying ephemeral Cloud Run Job: ${JOB_NAME}"
gcloud alpha run jobs create "${JOB_NAME}" \
  --project="${PROJECT_ID}" \
  --region="${REGION}" \
  --image="${IMAGE_TAG}" \
  --functional-type=agent \
  --identity-type=agent-identity \
  --identity-certificate \
  --set-env-vars="GOOGLE_CLOUD_PROJECT=${PROJECT_ID},GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME=${BUCKET_NAME}" \
  --max-retries=0 \
  --task-timeout=5m \
  --quiet
JOB_CREATED="true"

io::log_h2 "Executing Cloud Run Job: ${JOB_NAME}"
gcloud run jobs execute "${JOB_NAME}" \
  --project="${PROJECT_ID}" \
  --region="${REGION}" \
  --wait

io::log_green "Managed Workload Identity (MWLID) Cloud Run tests completed successfully!"
