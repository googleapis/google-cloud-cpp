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

readonly CACHE_KEYFILE="${KOKORO_GFILE_DIR:-/dev/shm}/build-results-service-account.json"

cache_download_enabled() {
  if [[ ! -f "${CACHE_KEYFILE}" ]]; then
    echo "================================================================"
    io::log "Service account for cache access is not configured."
    io::log "No attempt will be made to download the cache, exit with success."
    return 1
  fi

  if [[ "${RUNNING_CI:-}" != "yes" || (\
    "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GERRIT_ON_BORG" && \
    "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GITHUB") ]]; then
    echo "================================================================"
    io::log "Cache not downloaded as this is not a PR build."
    return 1
  fi
  return 0
}

cache_gcloud_cleanup() {
  revoke_service_account_keyfile "${CACHE_KEYFILE}" || true
  delete_gcloud_config || true
}

cache_download_tarball() {
  local -r GCS_FOLDER="$1"
  local -r DESTINATION="$2"
  local -r FILENAME="$3"

  trap cache_gcloud_cleanup RETURN
  create_gcloud_config
  activate_service_account_keyfile "${CACHE_KEYFILE}"

  echo "================================================================"
  io::log "gsutil configuration"
  gsutil version -l
  io::log "Downloading build cache ${FILENAME} from ${GCS_FOLDER}"
  env "CLOUDSDK_ACTIVE_CONFIG_NAME=${GCLOUD_CONFIG}" \
    gsutil "${CACHE_GSUTIL_DEBUG:--q}" \
    cp "gs://${GCS_FOLDER}/${FILENAME}" "${DESTINATION}"
}

cache_upload_enabled() {
  if [[ ! -f "${CACHE_KEYFILE}" ]]; then
    echo "================================================================"
    io::log "Service account for cache access is not configured."
    io::log "No attempt will be made to upload the cache, exit with success."
    return 1
  fi

  if [[ "${RUNNING_CI:-}" != "yes" || "${KOKORO_JOB_TYPE:-}" != "CONTINUOUS_INTEGRATION" ]]; then
    echo "================================================================"
    io::log "Cache not updated as this is not a CI build or it is a PR build."
    return 1
  fi
  return 0
}

cache_upload_tarball() {
  local -r SOURCE_DIRECTORY="$1"
  local -r FILENAME="$2"
  local -r GCS_FOLDER="$3"

  echo "================================================================"
  io::log "gsutil configuration"
  gsutil version -l
  io::log "Uploading build cache ${FILENAME} to ${GCS_FOLDER}"

  trap cache_gcloud_cleanup RETURN
  create_gcloud_config
  activate_service_account_keyfile "${CACHE_KEYFILE}"
  env "CLOUDSDK_ACTIVE_CONFIG_NAME=${GCLOUD_CONFIG}" \
    gsutil "${CACHE_GSUTIL_DEBUG:--q}" \
    cp "${SOURCE_DIRECTORY}/${FILENAME}" "gs://${CACHE_FOLDER}/"

  io::log "Upload completed"
}
