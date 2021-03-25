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
# ## Run builds using Google Cloud Build ##
#
# This script runs builds using Google Cloud Build. It expects a single
# argument of the build name to run. Valid build names are the basenames of the
# scripts in the `builds/` directory. An optional `--distro=<distro>` flag may
# be specified to indicate which `<distro>.Dockerfile` to run the named build
# in, and this build is submitted to Cloud Build to run. A distro of "local"
# (the default) indicates to run the build locally, without docker, using the
# invoking user's operating system and environment.
#
# Usage: build.sh [options] <build-name>
#
#   Options:
#     -d|--distro=<name>   The distro name to use
#     -p|--project=<name>  The Cloud Project ID to use
#     -l|--local           Run the build in the local environment
#     -h|--help            Print this help message
#
#   Build names:

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
  basename -s .sh ci/cloudbuild/builds/*.sh | sort | xargs printf "    %s\n"
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="d:p:lh" \
  --longoptions="distro:,project:,local,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

DISTRO=""
PROJECT_ID=""
LOCAL_BUILD="false"
while true; do
  case "$1" in
  -d | --distro)
    DISTRO="$2"
    shift 2
    ;;
  -p | --project)
    PROJECT_ID="$2"
    shift 2
    ;;
  -l | --local)
    LOCAL_BUILD="true"
    shift
    ;;
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
readonly DISTRO
readonly PROJECT_ID

if (($# == 0)); then
  print_usage
  exit 1
fi
readonly BUILD_NAME="$1"

if [[ "${LOCAL_BUILD}" = "true" ]]; then
  io::log_h1 "Starting local build: ${BUILD_NAME}"
  readonly TIMEFORMAT="==> 🕑 ${BUILD_NAME} completed in %R seconds"
  time "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh"
elif [[ -n "${DISTRO}" ]]; then
  # Quick checks to surface invalid arguments early
  if [ ! -r "ci/cloudbuild/${DISTRO}.Dockerfile" ]; then
    echo "Unknown distro: ${DISTRO}"
    print_usage
    exit 1
  elif [ ! -x "ci/cloudbuild/builds/${BUILD_NAME}.sh" ]; then
    echo "Unknown build name: ${BUILD_NAME}"
    print_usage
    exit 1
  fi
  account="$(gcloud config list account --format "value(core.account)")"
  subs="_DISTRO=${DISTRO}"
  subs+=",_BUILD_NAME=${BUILD_NAME}"
  subs+=",_CACHE_TYPE=manual-${account}"
  io::log_h1 "Starting cloud build: ${BUILD_NAME}"
  io::log "Substitutions ${subs}"
  args=(
    "--config=ci/cloudbuild/cloudbuild.yaml"
    "--substitutions=${subs}"
  )
  if [[ -n "${PROJECT_ID}" ]]; then
    args+=("--project=${PROJECT_ID}")
  fi
  exec gcloud builds submit "${args[@]}" .
else
  echo "Invalid arguments"
  print_usage
  exit 1
fi
