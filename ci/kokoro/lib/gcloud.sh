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

# Make our include guard clean against set -o nounset.
test -n "${CI_KOKORO_LIB_GCLOUD_SH__:-}" || declare -i CI_KOKORO_LIB_GCLOUD_SH__=0
if ((CI_KOKORO_LIB_GCLOUD_SH__++ != 0)); then
  return 0
fi # include guard

source module /ci/lib/io.sh

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

GCLOUD_BIN="/usr/local/google-cloud-sdk/bin/gcloud"
if [ ! -x "${GCLOUD_BIN}" ]; then
  # If the bin wasn't found at the above path, hope it's found in ${PATH}.
  GCLOUD_BIN="gcloud"
fi
readonly GCLOUD_BIN

activate_service_account_keyfile() {
  local -r keyfile="$1"
  local -r account="$(sed -n 's/.*"client_email": "\(.*\)",.*/\1/p' "${keyfile}")"
  "${GCLOUD_BIN}" "${GCLOUD_ARGS[@]}" auth activate-service-account \
    --key-file "${keyfile}" >/dev/null
  "${GCLOUD_BIN}" "${GCLOUD_ARGS[@]}" config set account "${account}"
}

revoke_service_account_keyfile() {
  local -r keyfile="$1"
  local -r account="$(sed -n 's/.*"client_email": "\(.*\)",.*/\1/p' "${keyfile}")"
  "${GCLOUD_BIN}" "${GCLOUD_ARGS[@]}" auth revoke "${account}" >/dev/null
}

delete_gcloud_config() {
  "${GCLOUD_BIN}" --quiet config configurations delete "${GCLOUD_CONFIG}"
}

create_gcloud_config() {
  if ! "${GCLOUD_BIN}" --quiet config configurations \
    describe "${GCLOUD_CONFIG}" >/dev/null 2>&1; then
    io::log "Create the gcloud configuration for the cloud-cpp tests."
    "${GCLOUD_BIN}" --quiet --no-user-output-enabled config configurations \
      create --no-activate "${GCLOUD_CONFIG}" >/dev/null
  fi
  if [[ -n "${GOOGLE_CLOUD_PROJECT:-}" ]]; then
    # At this point, we haven't yet activated our service account, but we also
    # don't want this config setting to use any default account that may
    # already exist on the machine, so we explicitly set `--account=""` to
    # avoid using incidental gcloud accounts.
    "${GCLOUD_BIN}" "${GCLOUD_ARGS[@]}" --account="" config set project "${GOOGLE_CLOUD_PROJECT}"
  fi
}
