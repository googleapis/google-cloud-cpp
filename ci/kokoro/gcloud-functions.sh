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

if [[ -z "${GCLOUD_CONFIG:-}" ]]; then
  readonly GCLOUD_CONFIG="cloud-cpp-integration"
fi

if [[ -z "${GCLOUD_ARGS:-}" ]]; then
  readonly GCLOUD_ARGS=(
    # Do not seek confirmation for any actions, assume the default
    "--quiet"

    # Run the command using a custom configuration, this avoids affecting the
    # user's `default` configuration
    "--configuration=${GCLOUD_CONFIG}"
  )
fi

activate_service_account_keyfile() {
  local -r keyfile="$1"
  local -r account="$(sed -n 's/.*"client_email": "\(.*\)",.*/\1/p' "${keyfile}")"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth activate-service-account \
    --key-file "${keyfile}" >/dev/null
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" config set account "${account}"
}

revoke_service_account_keyfile() {
  local -r keyfile="$1"
  local -r account="$(sed -n 's/.*"client_email": "\(.*\)",.*/\1/p' "${keyfile}")"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth revoke "${account}" >/dev/null
}

delete_gcloud_config() {
  "${GCLOUD}" --quiet config configurations delete "${GCLOUD_CONFIG}"
}

create_gcloud_config() {
  if ! "${GCLOUD}" --quiet config configurations \
    describe "${GCLOUD_CONFIG}" >/dev/null 2>&1; then
    echo
    echo "================================================================"
    log_normal "Create the gcloud configuration for the cloud-cpp tests."
    "${GCLOUD}" --quiet --no-user-output-enabled config configurations \
      create --no-activate "${GCLOUD_CONFIG}" >/dev/null
  fi
  if [[ -n "${GOOGLE_CLOUD_PROJECT:-}" ]]; then
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" config set project "${GOOGLE_CLOUD_PROJECT}"
  fi
}

cleanup_hmac_service_account() {
  local -r ACCOUNT="$1"
  log_normal "Deleting account ${ACCOUNT}"
  # We can ignore errors here, sometime the account exists, but the bindings
  # are gone (or were never created). The binding is harmless if the account
  # is deleted.
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" projects remove-iam-policy-binding \
    "${GOOGLE_CLOUD_PROJECT}" \
    --member "serviceAccount:${ACCOUNT}" \
    --role roles/iam.serviceAccountTokenCreator >/dev/null || true
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts delete \
    "${ACCOUNT}" >/dev/null
}

cleanup_stale_hmac_service_accounts() {
  # The service accounts created below start with hmac-YYYYMMDD-, we list the
  # accounts with that prefix, and with a date from at least 2 days ago to
  # find and remove any stale accounts.
  local THRESHOLD
  THRESHOLD="$(date +%Y%m%d --date='2 days ago')"
  readonly THRESHOLD
  local email
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts list \
    --filter="email~^hmac-[0-9]{8}- AND email<hmac-${THRESHOLD}-" \
    --format='csv(email)[no-heading]' |
    while read -r email; do
      cleanup_hmac_service_account "${email}"
    done
}

create_hmac_service_account() {
  local -r ACCOUNT="$1"
  local -r EMAIL="${ACCOUNT}@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts create "${ACCOUNT}"
  log_normal "Grant service account permissions to create HMAC keys."
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" projects add-iam-policy-binding \
    "${GOOGLE_CLOUD_PROJECT}" \
    --member "serviceAccount:${EMAIL}" \
    --role roles/iam.serviceAccountTokenCreator >/dev/null
}
