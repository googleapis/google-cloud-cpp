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

if [[ $# != 3 ]]; then
  echo "Usage: $(basename "$0") <cache-folder> <cache-name> <home-directory>"
  exit 1
fi

readonly CACHE_FOLDER="$1"
readonly CACHE_NAME="$2"
readonly HOME_DIR="$3"

readonly KEYFILE="${KOKORO_GFILE_DIR:-/dev/shm}/build-results-service-account.json"
if [[ ! -f "${KEYFILE}" ]]; then
  echo "================================================================"
  echo "Service account for cache access is not configured."
  echo "No attempt will be made to download the cache, exit with success."
  exit 0
fi

if [[ "${KOKORO_JOB_TYPE:-}" == "PRESUBMIT_GERRIT_ON_BORG" ]] || \
   [[ "${KOKORO_JOB_TYPE:-}" == "PRESUBMIT_GITHUB" ]]; then
  echo "================================================================"
  echo "This is a presubmit build, cache will not be updated, exist with success."
  exit 0
fi

echo "================================================================"
echo "$(date -u): Uploading build cache ${CACHE_NAME} to ${CACHE_FOLDER}"

set -v
tar -zcf "${HOME_DIR}/${CACHE_NAME}.tar.gz" \
    "${HOME_DIR}/.cache" "${HOME_DIR}/.ccache"
gcloud --quiet auth activate-service-account --key-file "${KEYFILE}"
gsutil -q cp "${HOME_DIR}/${CACHE_NAME}.tar.gz" "gs://${CACHE_FOLDER}/"
gcloud --quiet auth revoke --all >/dev/null 2>&1 || echo "Ignore revoke failure"

exit 0
