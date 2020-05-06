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

if [[ "${CHECK_MARKDOWN}" != "yes" ]]; then
  echo "Skipping the generated markdown check as it is disabled for this build."
  exit 0
fi

declare -A -r GENERATOR_MAP=(
  ["ci/generate-markdown/generate-readme.sh"]="README.md"
  ["ci/generate-markdown/generate-bigtable-readme.sh"]="google/cloud/bigtable/README.md"
  ["ci/generate-markdown/generate-spanner-readme.sh"]="google/cloud/spanner/README.md"
  ["ci/generate-markdown/generate-storage-readme.sh"]="google/cloud/storage/README.md"
  ["ci/generate-markdown/generate-packaging.sh"]="doc/packaging.md"
)

exit_status=0
for k in "${!GENERATOR_MAP[@]}"; do
  generator="${k}"
  file="${GENERATOR_MAP[${k}]}"
  out=$(diff <("${generator}") "${file}" || true)
  if [[ -n "${out}" ]]; then
    echo "Did you forget to run ${generator}?"
    echo "Unexpected diff found in ${file}:"
    echo "$out"
    exit_status=1
  fi
done

exit ${exit_status}
