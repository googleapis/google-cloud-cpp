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

lib="google/cloud/${LIBRARY}"
if [[ ! -r "${lib}/doc/main.dox" ]]; then
  exit 0
fi

inject_start="<!-- inject-endpoint-env-vars-start -->"
inject_end="<!-- inject-endpoint-env-vars-end -->"
env_vars=("")
while IFS= read -r -d $'\0' option_defaults_cc; do
  service="$(basename "${option_defaults_cc}" _option_defaults.cc)"
  connection_h="${lib}/${service}_connection.h"
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
done < <(git ls-files -z -- "${lib}/internal/*_option_defaults.cc")
sed -i -f - "${lib}/doc/main.dox" <<EOT
/${inject_start}/,/${inject_end}/c \\
${inject_start}\\
$(printf '%s\\\n' "${env_vars[@]}")
${inject_end}
EOT
