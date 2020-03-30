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

mkdir -p "${HOME}/.ccache"
[[ -f "${HOME}/.ccache/ccache.conf" ]] || \
  echo "max_size = 4.0G" >"${HOME}/.ccache/ccache.conf"

KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

readonly KEYFILE="${KOKORO_GFILE_DIR}/build-results-service-account.json"
if [[ ! -f "${KEYFILE}" ]]; then
  echo "================================================================"
  echo "Service account for cache access is not configured."
  echo "No attempt will be made to download the cache, exit with success."
  exit 0
fi

if [[ "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GERRIT_ON_BORG" ]] && \
   [[ "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GITHUB" ]]; then
  echo "================================================================"
  echo "This is not a presubmit build, not using the cache."
  exit 0
fi

echo "================================================================"
echo "$(date -u): Downloading build cache ${CACHE_NAME} from ${CACHE_FOLDER}"
readonly DOWNLOAD="cmake-out/download"
mkdir -p "${DOWNLOAD}"
gcloud --quiet auth activate-service-account --key-file "${KEYFILE}"
gsutil -q cp "gs://${CACHE_FOLDER}/${CACHE_NAME}.tar.gz" "${DOWNLOAD}"

ACCOUNT="$(sed -n 's/.*"client_email": "\(.*\)",.*/\1/p' "${KEYFILE}")"
readonly ACCOUNT
gcloud --quiet auth revoke "${ACCOUNT}" >/dev/null 2>&1 || \
    echo "Ignore revoke failure"

echo "================================================================"
echo "$(date -u): Extracting build cache ${CACHE_NAME}"
# Ignore timestamp warnings, Bazel has files with timestamps 10 years
# into the future :shrug:
tar -C / -zxf "${DOWNLOAD}/${CACHE_NAME}.tar.gz" 2>&1 | \
    grep -E -v 'tar:.*in the future'

echo "================================================================"
echo "$(date -u): build cache extracted"

exit 0
