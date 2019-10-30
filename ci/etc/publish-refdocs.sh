#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

# This script is meant to source from ci/kokoro/docker/publish_refdocs.sh.

upload_docs "google-cloud-bigtable" "${BUILD_OUTPUT}/google/cloud/bigtable/html" \
  "bigtable.tag" "${BRANCH}" "${CREDENTIALS_FILE}" "${STAGING_BUCKET}"

upload_docs "google-cloud-storage" "${BUILD_OUTPUT}/google/cloud/storage/html" \
  "storage.tag" "${BRANCH}" "${CREDENTIALS_FILE}" "${STAGING_BUCKET}"
