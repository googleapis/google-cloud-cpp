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

mkdir -p "${HOME_DIR}/.cache"
mkdir -p "${HOME_DIR}/.ccache"
echo "max_size = 4.0G" >"${HOME_DIR}/.ccache/ccache.conf"

readonly KEYFILE="${KOKORO_GFILE_DIR:-/dev/shm}/build-results-service-account.json"
if [[ ! -f "${KEYFILE}" ]]; then
  echo "================================================================"
  echo "Service account for cache access is not configured."
  echo "No attempt will be made to download the cache, exit with success."
  exit 0
fi

echo "================================================================"
echo "$(date -u): Downloading build cache ${CACHE_NAME} from ${CACHE_FOLDER}"

set -v
gcloud --quiet auth activate-service-account --key-file "${KEYFILE}"
gsutil -q cp "gs://${CACHE_FOLDER}/${CACHE_NAME}.tar.gz" "${HOME_DIR}"
gcloud --quiet auth revoke --all || echo "Ignore revoke failure"
# Ignore timestamp warnings, Bazel has files with timestamps 10 years
# into the future :shrug:
tar -zxf "${HOME_DIR}/${CACHE_NAME}.tar.gz" 2>&1 | grep -E -v 'tar:.*in the future'

exit 0
