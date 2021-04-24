#!/bin/bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/lib/io.sh
source module ci/cloudbuild/builds/lib/git.sh

io::log_h2 "Generating Markdown"
declare -A -r GENERATOR_MAP=(
  ["ci/generate-markdown/generate-readme.sh"]="README.md"
  ["ci/generate-markdown/generate-bigtable-readme.sh"]="google/cloud/bigtable/README.md"
  ["ci/generate-markdown/generate-pubsub-readme.sh"]="google/cloud/pubsub/README.md"
  ["ci/generate-markdown/generate-spanner-readme.sh"]="google/cloud/spanner/README.md"
  ["ci/generate-markdown/generate-storage-readme.sh"]="google/cloud/storage/README.md"
  ["ci/generate-markdown/generate-packaging.sh"]="doc/packaging.md"
)
for generator in "${!GENERATOR_MAP[@]}"; do
  "${generator}" >"${GENERATOR_MAP[${generator}]}"
done
# Any diffs in the markdown files result in a checker failure.
# Edited files are left in the repo so they can be committed.
git diff --color --exit-code "${GENERATOR_MAP[@]}"
io::log_green "Markdown OK"

io::log_h2 "Checking Style"
CHECK_STYLE=yes NCPU="$(nproc)" RUNNING_CI="${GOOGLE_CLOUD_BUILD:-no}" \
  ci/check-style.sh
