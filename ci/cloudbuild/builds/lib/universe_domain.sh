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

# Only create the SA key file if the secret is available.
if [[ -n "${UD_SERVICE_ACCOUNT}" ]]; then
  ORIG_UMASK=$(umask)
  umask 077
  UD_SA_KEY_FILE=$(mktemp)
  echo "${UD_SERVICE_ACCOUNT}" >"${UD_SA_KEY_FILE}"
  umask "${ORIG_UMASK}"
fi

function ud::bazel_run() {
  io::log "Executing bazel run $1 with obscured arguments:"
  bazel run --ui_event_filters=-info -- "$@"
}

function ud::bazel_test() {
  io::log "Executing bazel test $1 with obscured arguments:"
  bazel test --test_env=UD_SA_KEY_FILE="${UD_SA_KEY_FILE}" \
    --test_env=UD_REGION="${UD_REGION}" \
    --test_env=UD_PROJECT="${UD_PROJECT}" -- "$@"
}
