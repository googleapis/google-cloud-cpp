#!/bin/bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../../lib/init.sh"
source module ci/lib/io.sh
cd "${PROGRAM_DIR}"

readonly GOOGLE_CLOUD_PROJECT="cloud-cpp-testing-resources"
readonly IMAGE="gcr.io/${GOOGLE_CLOUD_PROJECT}/send-build-alerts"

io::log_h1 "Building code with pack"
gcloud builds submit --project="${GOOGLE_CLOUD_PROJECT}" \
  --pack="image=${IMAGE},env=GOOGLE_FUNCTION_TARGET=SendBuildAlerts"

io::log_h1 "Deploying to Cloud Run"
gcloud run deploy send-build-alerts \
  --project="${GOOGLE_CLOUD_PROJECT}" \
  --image="${IMAGE}:latest" \
  --region="us-central1" \
  --platform="managed" \
  --set-secrets=GCB_BUILD_ALERT_WEBHOOK=GCB_BUILD_ALERT_WEBHOOK:latest,GCB_FRIENDS_BUILD_ALERT_WEBHOOK=GCB_FRIENDS_BUILD_ALERT_WEBHOOK:latest \
  --no-allow-unauthenticated
