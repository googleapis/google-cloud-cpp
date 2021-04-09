#!/bin/bash
#
# Copyright 2021 Google LLC
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
# There are a few commmands that need to be run in order to re-deploy a change
# to the bigquery.yaml file, and this script runs those commands. These
# commands are enough to RE-deploy the service, but there are about 10
# additional commands needed to set up the service in the first place. See
# https://cloud.google.com/build/docs/configuring-notifications/configure-bigquery
# for more details about the first-time setup that's needed if we ever move
# this to a new cloud project.

set -euo pipefail

source "$(dirname "$0")/../../../lib/init.sh"
source module ci/lib/io.sh
cd "${PROGRAM_DIR}"

readonly BUCKET="gs://cloud-cpp-testing-resources-configs/google-cloud-cpp"
io::log_h1 "Copying bigquery.yaml to ${BUCKET}"
gsutil cp bigquery.yaml gs://cloud-cpp-testing-resources-configs/google-cloud-cpp

readonly SERVICE="google-cloud-cpp-gcb-bigquery-notifier"
io::log_h1 "Deploying Cloud Run service ${SERVICE}"
run_args=(
  "--image=us-east1-docker.pkg.dev/gcb-release/cloud-build-notifiers/bigquery:latest"
  "--no-allow-unauthenticated"
  "--update-env-vars=CONFIG_PATH=${BUCKET}/bigquery.yaml,PROJECT_ID=cloud-cpp-testing-resources"
  "--project=cloud-cpp-testing-resources"
  "--platform=managed"
  "--region=us-central1"
)
gcloud run deploy "${SERVICE}" "${run_args[@]}"

io::log_yellow "You can view the status at:" \
  "https://console.cloud.google.com/run?project=cloud-cpp-testing-resources"
