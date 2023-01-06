#!/usr/bin/env bash
#
# Copyright 2021 Google LLC
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

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

if [[ $# -ne 1 ]]; then
  echo 2>"Usage: $(basename "$0") <library>"
  exit 1
fi

cd "${PROJECT_ROOT}"
readonly LIBRARY=$1

readonly LIB="google/cloud/${LIBRARY}"
readonly MAIN_DOX="google/cloud/${LIBRARY}/doc/main.dox"
if [[ ! -r "${MAIN_DOX}" ]]; then
  exit 0
fi

inject_start="<!-- inject-endpoint-env-vars-start -->"
inject_end="<!-- inject-endpoint-env-vars-end -->"
env_vars=("")
while IFS= read -r -d $'\0' option_defaults_cc; do
  service="$(basename "${option_defaults_cc}" _option_defaults.cc)"
  service_dir="$(dirname "$(dirname "${option_defaults_cc}")")"
  connection_h="${service_dir}/${service}_connection.h"
  # Should we generate documentation for GOOGLE_CLOUD_CPP_.*_AUTHORITY too?
  variable_re='GOOGLE_CLOUD_CPP_.*_ENDPOINT'
  variable=$(grep -om1 "${variable_re}" "${option_defaults_cc}")
  endpoint_re='"[^"]*?\.googleapis\.com"'
  endpoint=$(grep -Pom1 "${endpoint_re}" "${option_defaults_cc}")
  if grep -q 'location,' "${option_defaults_cc}"; then
    endpoint="\"<location>-${endpoint:1:-1}\""
  fi
  make_connection_re='Make.*?Connection()'
  make_connection=$(grep -Pom1 "${make_connection_re}" "${connection_h}")
  env_vars+=("- \`${variable}=...\` overrides the")
  env_vars+=("  \`EndpointOption\` (which defaults to ${endpoint})")
  env_vars+=("  used by \`${make_connection}()\`.")
  env_vars+=("")
done < <(git ls-files -z -- "${LIB}/*_option_defaults.cc")

sed -i -f - "${MAIN_DOX}" <<EOT
/${inject_start}/,/${inject_end}/c \\
${inject_start}\\
$(printf '%s\\\n' "${env_vars[@]}")
${inject_end}
EOT

IFS= mapfile -d $'\0' -t samples_cc < <(git ls-files -z -- "${LIB}/*samples/*_client_samples.cc")

(
  sed '/<!-- inject-endpoint-snippet-start -->/q' "${MAIN_DOX}"
  if [[ ${#samples_cc[@]} -gt 0 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="$(sed -n '/main-dox-marker: / s;// main-dox-marker: \(.*\);\1;p' "${sample_cc}")"
    echo 'For example, this will override the default endpoint for `'"${client_name}"'`:'
    echo
    echo "@snippet $(basename "${sample_cc}") set-client-endpoint"
    if [[ ${#samples_cc[@]} -gt 1 ]]; then
      echo
      echo "Follow these links to find examples for other \\c *Client classes:"
      for sample_cc in "${samples_cc[@]}"; do
        sed -n "/main-dox-marker: / s;// main-dox-marker: \(.*\); [\1](@ref \1-endpoint-snippet);p" "${sample_cc}"
      done
    fi
  fi
  echo
  sed -n '/<!-- inject-endpoint-snippet-end -->/,$p' "${MAIN_DOX}"
) | sponge "${MAIN_DOX}"

(
  sed '/<!-- inject-service-account-snippet-start -->/q' "${MAIN_DOX}"
  if [[ ${#samples_cc[@]} -gt 0 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="$(sed -n '/main-dox-marker: / s;// main-dox-marker: \(.*\);\1;p' "${sample_cc}")"
    echo "@snippet $(basename "${sample_cc}") with-service-account"
    if [[ ${#samples_cc[@]} -gt 1 ]]; then
      echo
      echo "Follow these links to find examples for other \\c *Client classes:"
      for sample_cc in "${samples_cc[@]}"; do
        sed -n "/main-dox-marker: / s;// main-dox-marker: \(.*\); [\1](@ref \1-service-account-snippet);p" "${sample_cc}"
      done
    fi
  fi
  echo
  sed -n '/<!-- inject-service-account-snippet-end -->/,$p' "${MAIN_DOX}"
) | sponge "${MAIN_DOX}"

(
  sed '/<!-- inject-endpoint-pages-start -->/q' "${MAIN_DOX}"
  for sample_cc in "${samples_cc[@]}"; do
    client_name=$(sed -n "/main-dox-marker: / s;// main-dox-marker: \(.*\);\1;p" "${sample_cc}")
    printf '\n/*! @page %s-endpoint-snippet Override %s Endpoint Configuration\n\n@snippet %s set-client-endpoint\n\n*/\n' \
      "${client_name}" "${client_name}" "${sample_cc}"
    printf '\n/*! @page %s-service-account-snippet Override %s Authentication Defaults\n\n@snippet %s with-service-account\n\n*/\n' \
      "${client_name}" "${client_name}" "${sample_cc}"
  done
  sed -n '/<!-- inject-endpoint-pages-end -->/,$p' "${MAIN_DOX}"
) | sponge "${MAIN_DOX}"
