#!/bin/bash
#
# Copyright 2022 Google LLC
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
#
# This script restores the GCB (Google Cloud Build) triggers for a branch.
# This is useful when creating patch releases (including for LTS branches),
# where the CI matrix may have changed since the branch was created, and
# therefore the existing branches may not apply.
#
# Usage: convert-to-branch-triggers.sh [options] <branch>
#
#   Options:
#     -h|--help           Print this help message

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

function convert_triggers() {
  local branch="$1"
  local name_prefix="${branch//./-}"
  shift
  for file in "$@"; do
    sed \
      -e "s/^name: /name: ${name_prefix}-/" \
      -e "s/branch: .*/branch: ${branch}/" "${file}" |
      sponge "${file}"
  done
}
export -f convert_triggers

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="h" \
  --longoptions="help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

while true; do
  case "$1" in
  -h | --help)
    print_usage
    exit 0
    ;;
  --)
    shift
    break
    ;;
  esac
done

if [[ $# -lt 1 ]]; then
  print_usage
  exit 1
fi
BRANCH="$1"

git ls-files -z -- ci/cloudbuild/triggers/*.yaml |
  xargs -P "$(nproc)" -n 50 -0 bash -c "convert_triggers \"${BRANCH}\" \"\$@\""
