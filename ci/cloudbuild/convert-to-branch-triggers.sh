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

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  cat <<_EOM_
Usage: ${PROGRAM_NAME} [options]
  Options:
    -h|--help          Print this help message
    --branch=<name>    Use name instead of the current branch
_EOM_
}

function convert_triggers() {
  local branch="$1"
  local name_prefix="${branch//./-}"
  shift
  for file in "$@"; do
    sed -i \
      -e "s/^name: /name: ${name_prefix}-/" \
      -e "s/branch: .*/branch: ${branch}/" "${file}"
  done
}
export -f convert_triggers

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="h" \
  --longoptions="help,branch:" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

BRANCH=""
while true; do
  case "$1" in
    -h | --help)
      print_usage
      exit 0
      ;;
    --branch)
      BRANCH="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
  esac
done

if [[ -z "${BRANCH}" ]]; then
  BRANCH="$(git branch --show-current)"
  if [[ ! "${BRANCH}" =~ ^v[1-9]([0-9]*)?\.[0-9]+\.x$ ]]; then
    # This is not a release branch, find the parent branch.
    BRANCH="$(git show-branch |
      sed -n '/\*/ s/].*//' |
      grep -v "$(git rev-parse --abbrev-ref HEAD)" |
      sed '1,1 s/^.*\[//')"
  fi
fi

git ls-files -z -- ci/cloudbuild/triggers/*.yaml |
  xargs -P "$(nproc)" -n 10 -0 bash -c "convert_triggers \"${BRANCH}\" \"\$@\""
