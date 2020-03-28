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

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <cache-folder> <cache-name>"
  exit 1
fi

readonly CACHE_FOLDER="$1"
readonly CACHE_NAME="$2"

KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

readonly KEYFILE="${KOKORO_GFILE_DIR}/build-results-service-account.json"
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
# TODO(coryan) - restore this before MERGE:  exit 0
fi

readonly BAZEL_CACHE_DIR="/private/var/tmp/_bazel_${USER}"
readonly CCACHE_DIR="${HOME}/.ccache"

dirs=()
for dir in "${BAZEL_CACHE_DIR}" "${CCACHE_DIR}"; do
  if [[ -d "${dir}"  ]]; then dirs+=("${dir}"); fi
done

readonly UPLOAD="cmake-out/upload"
mkdir -p "${UPLOAD}"

echo "================================================================"
echo "$(date -u): Preparing cache tarball for ${CACHE_NAME}"
tar -C / -zcf "${UPLOAD}/${CACHE_NAME}.tar.gz" "${dirs[@]}"

echo "================================================================"
echo "$(date -u): Uploading build cache ${CACHE_NAME} to ${CACHE_FOLDER}"
gcloud --quiet auth activate-service-account --key-file "${KEYFILE}"
gsutil -q cp "${UPLOAD}/${CACHE_NAME}.tar.gz" "gs://${CACHE_FOLDER}/"
gcloud --quiet auth revoke --all >/dev/null 2>&1 || echo "Ignore revoke failure"

exit 0
