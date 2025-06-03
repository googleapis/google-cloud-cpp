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
readonly DOCDIR="google/cloud/${LIBRARY}/doc"
# Only generated libraries have a `main.dox`, exit successfully in other cases.
if [[ ! -r "${DOCDIR}/main.dox" ]]; then
  exit 0
fi
# For other libraries we expect a number of files to exist.
readonly EXPECTED=(
  "environment-variables.dox"
  "override-authentication.dox"
  "override-endpoint.dox"
  "override-retry-policies.dox"
)
for file in "${EXPECTED[@]}"; do
  if [[ ! -r "${DOCDIR}/${file}" ]]; then
    echo "Missing ${file} for in ${DOCDIR}"
    exit 1
  fi
done
readonly MAIN_DOX="${DOCDIR}/main.dox"
readonly ENVIRONMENT_DOX="${DOCDIR}/environment-variables.dox"
readonly OVERRIDE_AUTHENTICATION_DOX="${DOCDIR}/override-authentication.dox"
readonly OVERRIDE_ENDPOINT_DOX="${DOCDIR}/override-endpoint.dox"
readonly OVERRIDE_RETRY_POLICIES_DOX="${DOCDIR}/override-retry-policies.dox"

inject_start="<!-- inject-endpoint-env-vars-start -->"
inject_end="<!-- inject-endpoint-env-vars-end -->"
declare -A env_vars
while IFS= read -r -d $'\0' option_defaults_cc; do
  service="$(basename "${option_defaults_cc}" _option_defaults.cc)"
  service_dir="$(dirname "$(dirname "${option_defaults_cc}")")"
  connection_h="${service_dir}/${service}_connection.h"
  # Should we generate documentation for GOOGLE_CLOUD_CPP_.*_AUTHORITY too?
  variable_re='GOOGLE_CLOUD_CPP_.*_ENDPOINT'
  variable=$(grep -om1 "${variable_re}" "${option_defaults_cc}")
  endpoint_re='"[^"]*?\.googleapis\.com"'
  endpoint=$(grep -Pom1 "${endpoint_re}" "${option_defaults_cc}")
  if ! grep -q 'optional location tag' "${option_defaults_cc}"; then
    if grep -q 'location,' "${option_defaults_cc}"; then
      endpoint="\"<location>-${endpoint:1:-1}\""
    fi
  fi
  make_connection_re='Make.*?Connection()'
  make_connection=$(grep -Pom1 "${make_connection_re}" "${connection_h}")
  env_var=("")
  env_var+=("- \`${variable}=...\` overrides the")
  env_var+=("  \`EndpointOption\` (which defaults to ${endpoint})")
  env_var+=("  used by \`${make_connection}()\`.")
  env_vars[$(printf '%s\\\n' "${env_var[@]}")]=""
done < <(git ls-files -z -- "${LIB}/*_option_defaults.cc")
mapfile -d '' sorted_env_vars < <(printf '%s\0' "${!env_vars[@]}" | sort -z)
sed -i -f - "${ENVIRONMENT_DOX}" <<EOT
/${inject_start}/,/${inject_end}/c \\
${inject_start}\\
$(printf '%s\n' "${sorted_env_vars[@]}")
\\
${inject_end}
EOT

IFS= mapfile -d $'\0' -t samples_cc < <(git ls-files -z -- "${LIB}/*samples/*_client_samples.cc")

declare -A clients
declare -A lro_clients
for sample in "${samples_cc[@]}"; do
  clients[$sample]="$(sed -n '/main-dox-marker: / s;// main-dox-marker: \(.*\);\1;p' "${sample}")"
  lro_clients[$sample]="$(sed -n '/lro-marker: true/p' "${sample}")"
done

(
  sed '/<!-- inject-client-list-start -->/q' "${MAIN_DOX}"
  if [[ ${#samples_cc[@]} -eq 0 ]]; then
    true
  elif [[ ${#samples_cc[@]} -eq 1 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="${clients[${sample_cc}]}"
    cat <<_EOF_
The main class in this library is
[\`${client_name}\`](@ref google::cloud::${client_name}). All RPCs are exposed
as member functions of this class. Other classes provide helpers, configuration
parameters, and infrastructure to mock
[\`${client_name}\`](@ref google::cloud::${client_name}) when testing your
application.
_EOF_
  else
    cat <<'_EOF_'
This library offers multiple `*Client` classes, which are listed below. Each one
of these classes exposes all the RPCs for a service as member functions of the
class. This library groups multiple services because they are part of the same
product or are often used together. A typical example may be the administrative
and data plane operations for a single product.

The library also has other classes that provide helpers, configuration
parameters, and infrastructure to mock the `*Client` classes when testing your
application.

_EOF_
    for sample_cc in "${samples_cc[@]}"; do
      client_name="${clients[${sample_cc}]}"
      # shellcheck disable=SC2016
      printf -- '- [\c %s](@ref google::cloud::%s)\n' "${client_name}" "${client_name}"
    done
  fi
  sed -n '/<!-- inject-client-list-end -->/,$p' "${MAIN_DOX}"
) | sponge "${MAIN_DOX}"

(
  sed '/<!-- inject-service-account-snippet-start -->/q' "${OVERRIDE_AUTHENTICATION_DOX}"
  if [[ ${#samples_cc[@]} -gt 0 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="${clients[${sample_cc}]}"
    echo "@snippet $(basename "${sample_cc}") with-service-account"
    if [[ ${#samples_cc[@]} -gt 1 ]]; then
      echo
      echo "Follow these links to find examples for other \\c *Client classes:"
      echo
      for sample_cc in "${samples_cc[@]}"; do
        client_name="${clients[${sample_cc}]}"
        # shellcheck disable=SC2016
        printf -- '- [\c %s](@ref %s-service-account-snippet)\n' "${client_name}" "${client_name}"
      done
    fi
  fi
  echo
  sed -n '/<!-- inject-service-account-snippet-end -->/,$p' "${OVERRIDE_AUTHENTICATION_DOX}"
) | sponge "${OVERRIDE_AUTHENTICATION_DOX}"

(
  sed '/<!-- inject-authentication-pages-start -->/q' "${OVERRIDE_AUTHENTICATION_DOX}"
  for sample_cc in "${samples_cc[@]}"; do
    client_name="${clients[${sample_cc}]}"
    printf '\n/*! @page %s-service-account-snippet Override %s Authentication Defaults\n\n@snippet %s with-service-account\n\n*/\n' \
      "${client_name}" "${client_name}" "${sample_cc}"
  done
  sed -n '/<!-- inject-authentication-pages-end -->/,$p' "${OVERRIDE_AUTHENTICATION_DOX}"
) | sponge "${OVERRIDE_AUTHENTICATION_DOX}"

(
  sed '/<!-- inject-endpoint-snippet-start -->/q' "${OVERRIDE_ENDPOINT_DOX}"
  if [[ ${#samples_cc[@]} -gt 0 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="${clients[${sample_cc}]}"
    echo 'For example, this will override the default endpoint for `'"${client_name}"'`:'
    echo
    echo "@snippet $(basename "${sample_cc}") set-client-endpoint"
    if [[ ${#samples_cc[@]} -gt 1 ]]; then
      echo
      echo "Follow these links to find examples for other \\c *Client classes:"
      echo
      for sample_cc in "${samples_cc[@]}"; do
        client_name="${clients[${sample_cc}]}"
        # shellcheck disable=SC2016
        printf -- '- [\c %s](@ref %s-endpoint-snippet)\n' "${client_name}" "${client_name}"
      done
    fi
  fi
  echo
  sed -n '/<!-- inject-endpoint-snippet-end -->/,$p' "${OVERRIDE_ENDPOINT_DOX}"
) | sponge "${OVERRIDE_ENDPOINT_DOX}"

(
  sed '/<!-- inject-endpoint-pages-start -->/q' "${OVERRIDE_ENDPOINT_DOX}"
  for sample_cc in "${samples_cc[@]}"; do
    client_name="${clients[${sample_cc}]}"
    printf '\n/*! @page %s-endpoint-snippet Override %s Endpoint Configuration\n\n@snippet %s set-client-endpoint\n\n*/\n' \
      "${client_name}" "${client_name}" "${sample_cc}"
  done
  sed -n '/<!-- inject-endpoint-pages-end -->/,$p' "${OVERRIDE_ENDPOINT_DOX}"
) | sponge "${OVERRIDE_ENDPOINT_DOX}"

(
  sed '/<!-- inject-retry-snippet-start -->/q' "${OVERRIDE_RETRY_POLICIES_DOX}"
  if [[ ${#samples_cc[@]} -gt 0 ]]; then
    sample_cc="${samples_cc[0]}"
    client_name="${clients[${sample_cc}]}"
    cat <<_EOF_
For example, this will override the retry policies for \`${client_name}\`:

@snippet $(basename "${sample_cc}") set-retry-policy

This assumes you have created a custom idempotency policy. Such as:

@snippet $(basename "${sample_cc}") custom-idempotency-policy

_EOF_

    if [[ -n "${lro_clients[${sample_cc}]}" ]]; then
      cat <<_EOF_
This will override the polling policies for \`${client_name}\`

@snippet $(basename "${sample_cc}") set-polling-policy

_EOF_
    fi
    if [[ ${#samples_cc[@]} -gt 1 ]]; then
      echo
      echo "Follow these links to find examples for other \\c *Client classes:"
      echo
      for sample_cc in "${samples_cc[@]}"; do
        client_name="${clients[${sample_cc}]}"
        # shellcheck disable=SC2016
        printf -- '- [\c %s](@ref %s-retry-snippet)\n' "${client_name}" "${client_name}"
      done
    fi
  fi
  echo
  sed -n '/<!-- inject-retry-snippet-end -->/,$p' "${OVERRIDE_RETRY_POLICIES_DOX}"
) | sponge "${OVERRIDE_RETRY_POLICIES_DOX}"

(
  sed '/<!-- inject-retry-pages-start -->/q' "${OVERRIDE_RETRY_POLICIES_DOX}"
  for sample_cc in "${samples_cc[@]}"; do
    client_name="${clients[${sample_cc}]}"
    cat <<_EOF_

/*! @page ${client_name}-retry-snippet Override ${client_name} Retry Policies

This shows how to override the retry policies for ${client_name}:

@snippet ${sample_cc} set-retry-policy

Assuming you have created a custom idempotency policy. Such as:

@snippet ${sample_cc} custom-idempotency-policy

*/
_EOF_
  done
  sed -n '/<!-- inject-retry-pages-end -->/,$p' "${OVERRIDE_RETRY_POLICIES_DOX}"
) | sponge "${OVERRIDE_RETRY_POLICIES_DOX}"
