#!/bin/bash
#
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_UNIVERSE_DOMAIN_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_UNIVERSE_DOMAIN_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_UNIVERSE_DOMAIN_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/cloudbuild/builds/lib/bazel.sh

# Only create the SA key file if the secret is available.
if [[ -n "${UD_SERVICE_ACCOUNT}" ]]; then
  ORIG_UMASK=$(umask)
  umask 077
  UD_SA_KEY_FILE=$(mktemp)
  echo "${UD_SERVICE_ACCOUNT}" >"${UD_SA_KEY_FILE}"
  umask "${ORIG_UMASK}"
  io::log "Created SA key file ${UD_SA_KEY_FILE}"
fi

# Only create the EA key file if the secret is available.
if [[ -n "${UD_EXTERNAL_ACCOUNT_CRED}" ]]; then
  ORIG_UMASK=$(umask)
  umask 077
  UD_EA_KEY_FILE=$(mktemp)
  external_account_cred_template="${UD_EXTERNAL_ACCOUNT_CRED}"

  response=$(eval "${UD_FETCH_OIDC_TOKEN}")
  id_token=$(echo "${response}" | jq -r '.id_token')
  id_token_file=$(mktemp)
  echo "${id_token}" >"${id_token_file}"

  # shellcheck disable=SC2059
  printf "${external_account_cred_template}" "${id_token_file}" >"${UD_EA_KEY_FILE}"
  umask "${ORIG_UMASK}"
  io::log "Created EA key file ${UD_EA_KEY_FILE}"
fi

# Only create the IdToken SA key file if the secret is available.
if [[ -n "${UD_IDTOKEN_SA_IMPERSONATION_CRED}" ]]; then
  ORIG_UMASK=$(umask)
  umask 077
  UD_IDTOKEN_SA_KEY_FILE=$(mktemp)
  echo "${UD_IDTOKEN_SA_IMPERSONATION_CRED}" >"${UD_IDTOKEN_SA_KEY_FILE}"
  umask "${ORIG_UMASK}"
  io::log "Created IdToken SA key file ${UD_IDTOKEN_SA_KEY_FILE}"
fi

function ud::bazel_run() {
  io::log "Executing bazel run $1 with obscured arguments:"
  bazel run --ui_event_filters=-info -- "$@"
}

function ud::bazel_test() {
  mapfile -t args < <(bazel::common_args)
  io::log "Executing bazel test $1 with obscured arguments:"
  bazel test "${args[@]}" --sandbox_add_mount_pair=/tmp \
    --test_env=UD_SA_KEY_FILE="${UD_SA_KEY_FILE}" \
    --test_env=UD_EA_KEY_FILE="${UD_EA_KEY_FILE}" \
    --test_env=UD_REGION="${UD_REGION}" \
    --test_env=UD_ZONE="${UD_ZONE}" \
    --test_env=UD_IMPERSONATED_SERVICE_ACCOUNT_NAME="${UD_IMPERSONATED_SERVICE_ACCOUNT_NAME}" \
    --test_env=UD_IDTOKEN_SA_KEY_FILE="${UD_IDTOKEN_SA_KEY_FILE}" \
    --test_env=UD_PROJECT="${UD_PROJECT}" -- "$@"
}
