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

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(
    cd "$(dirname "$0")/../../.."
    pwd
  )"
fi
GCLOUD=gcloud
source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"

readonly CACHE_FOLDER="$1"
readonly CACHE_NAME="$2"

mkdir -p "${HOME}/.ccache"
[[ -f "${HOME}/.ccache/ccache.conf" ]] ||
  echo "max_size = 4.0G" >"${HOME}/.ccache/ccache.conf"

KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

readonly KEYFILE="${KOKORO_GFILE_DIR}/build-results-service-account.json"
if [[ ! -f "${KEYFILE}" ]]; then
  echo "================================================================"
  log_normal "Service account for cache access is not configured."
  log_normal "No attempt will be made to download the cache, exit with success."
  exit 0
fi

if [[ "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GERRIT_ON_BORG" ]] &&
  [[ "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GITHUB" ]]; then
  echo "================================================================"
  log_normal "Cache not downloaded as this is not a PR build."
  exit 0
fi

trap cleanup EXIT
cleanup() {
  revoke_service_account_keyfile "${KEYFILE}" || true
  delete_gcloud_config
}

readonly DOWNLOAD="cmake-out/download"
mkdir -p "${DOWNLOAD}"

create_gcloud_config
activate_service_account_keyfile "${KEYFILE}"

echo "================================================================"
log_normal "Downloading build cache ${CACHE_NAME} from ${CACHE_FOLDER}"
env "CLOUDSDK_ACTIVE_CONFIG_NAME=${GCLOUD_CONFIG}" \
  gsutil -q cp "gs://${CACHE_FOLDER}/${CACHE_NAME}.tar.gz" "${DOWNLOAD}"

echo "================================================================"
log_normal "Extracting build cache"
# Ignore timestamp warnings, Bazel has files with timestamps 10 years
# into the future :shrug:
tar -C / -zxf "${DOWNLOAD}/${CACHE_NAME}.tar.gz" 2>&1 |
  grep -E -v 'tar:.*in the future'
log_normal "Extraction completed"

exit 0
