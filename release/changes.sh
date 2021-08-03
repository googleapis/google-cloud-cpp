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

set -euo pipefail

source "$(dirname "$0")/../ci/lib/init.sh"
cd "${PROJECT_ROOT}"

# Lists all the changes between the given tag and HEAD.
function list_changes() {
  local since_tag="$1"
  shift
  git log --no-merges --format="format:%s" \
    "${since_tag}"..HEAD upstream/main -- "$@"
}

# Removes changes that are likely uninteresting to customers. Changes that
# indicate they're breaking with a `!:` are always kept. Pull request numbers
# of the form `(#NNNN)` are turned into links.
function filter_messages() {
  pr_url="https://github.com/googleapis/google-cloud-cpp/pull"
  for message in "$@"; do
    # Linkify "(#NNNN)" PR numbers.
    # shellcheck disable=SC2001
    message="$(sed -e "s,(#\([0-9]\+\)),([#\1](${pr_url}/\1)),g" <<<"${message}")"
    if grep -qP "!:" <<<"${message}"; then
      echo "${message}"
      continue
    fi
    # Likely uninteresting changes.
    if grep -qP "^(ci|chore|test|testing|cleanup)\b" <<<"${message}"; then
      continue
    fi
    echo "${message}"
  done
}

# Prints the given changelog IFF it's not empty. The first argument is the
# title for the section, the second argument is the URL to link the title to,
# and the remaining arguments are the changelog messages to be bulleted after.
function print_changelog() {
  local title=$1
  local url=$2
  shift 2
  if [[ $# -gt 0 ]]; then
    printf "\n### [%s](%s)\n\n" "${title}" "${url}"
    printf "* %s\n" "${changelog[@]}"
  fi
}

# We'll look for changes between this tag (likely the last release) and HEAD.
last_tag="$(git describe --tags --abbrev=0 upstream/main)"

# The format is: <title>,<path>[,<extra path> ...]
sections=(
  "BigQuery,google/cloud/bigquery"
  "Bigtable,google/cloud/bigtable"
  "Firestore,google/cloud/firestore"
  "IAM,google/cloud/iam"
  "Pub/Sub,google/cloud/pubsub"
  "Storage,google/cloud/storage"
  "Spanner,google/cloud/spanner"
)

# Adds a section for "Common Libraries", which excludes the previous sections.
mapfile -t exclude < <(printf "%s\n" "${sections[@]}" |
  cut -f2 -d, | sed -e 's/^/:(exclude)/')
sections+=("Common Libraries,google/cloud,${exclude[*]}")

for section in "${sections[@]}"; do
  title="$(cut -f1 -d, <<<"${section}")"
  path="$(cut -f2 -d, <<<"${section}")"
  mapfile -d ' ' -t extra < <(cut -f3 -d, <<<"${section}")
  url="https://github.com/googleapis/google-cloud-cpp/blob/main/${path}/README.md"
  mapfile -t messages < <(list_changes "${last_tag}" "${path}" "${extra[@]}")
  mapfile -t changelog < <(filter_messages "${messages[@]}")
  print_changelog "${title}" "${url}" "${changelog[@]}"
done
