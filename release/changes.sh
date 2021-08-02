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
#
# Usage:
#   $ changes.sh
#
# Prints a summary of changes since the last release in Markdown. PRs that seem
# uninteresting to customers are omitted. The output should be pasted into the
# `CHANGELOG.md` file during a release and manually tweaked as needed.

set -eu

function filter_messages() {
  pr_url="https://github.com/googleapis/google-cloud-cpp/pull"
  for message in "$@"; do
    # Linkify "(#NNNN)" PR numbers.
    # shellcheck disable=SC2001
    message="$(sed -e "s,(#\([0-9]\+\)),([#\1][${pr_url}/\1]),g" <<<"${message}")"
    # Include any message that says it's a breaking change, like "feat!: foo"
    if grep -qP "!:" <<<"${message}"; then
      echo "${message}"
      continue
    fi
    # Skip PRs that likely won't interest our customers.
    if grep -qP "^\*\s(ci|chore|test|testing|cleanup)\b" <<<"${message}"; then
      continue
    fi
    echo "${message}"
  done
}

# We'll look for changes between this tag (likely the last release) and HEAD.
last_tag="$(git describe --tags --abbrev=0 upstream/main)"

# The list of libraries to include in the CHANGELOG.md. The format is:
#   <section title>:<repo path>
libraries=(
  "BigQuery:google/cloud/bigquery"
  "Bigtable:google/cloud/bigtable"
  "Firestore:google/cloud/firestore"
  "IAM:google/cloud/iam"
  "Pub/Sub:google/cloud/pubsub"
  "Storage:google/cloud/storage"
  "Spanner:google/cloud/spanner"
)

# We'll add an entry to this array for each library path above.
git_exclude=()

for lib in "${libraries[@]}"; do
  title="$(cut -f1 -d: <<<"${lib}")"
  path="$(cut -f2 -d: <<<"${lib}")"
  url="https://github.com/googleapis/google-cloud-cpp/blob/main/${path}/README.md"
  mapfile -t messages < <(git log --no-merges --format="format:* %s" \
    "${last_tag}"..HEAD upstream/main -- "${path}")
  mapfile -t changelog < <(filter_messages "${messages[@]}")
  if [[ ${#changelog[@]} -gt 0 ]]; then
    printf "\n### [%s][%s]\n\n" "${title}" "${url}"
    printf "%s\n" "${changelog[@]}"
  fi
  git_exclude+=(":(exclude)${path}")
done

# Prints the changelog for the "common" library by excluding changes for all
# the libraries above.
mapfile -t messages < <(git log --no-merges --format="format:* %s" \
  "${last_tag}"..HEAD upstream/main -- "google/cloud" "${git_exclude[@]}")
mapfile -t changelog < <(filter_messages "${messages[@]}")
if [[ ${#changelog[@]} -gt 0 ]]; then
  url="https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/README.md"
  printf "\n### [%s][%s]\n\n" "Common Libraries" "${url}"
  printf "%s\n" "${changelog[@]}"
fi
