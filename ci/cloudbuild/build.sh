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
# be specified to indicate which `Dockerfile.<distro>` to run the named build
# in, and this build is submitted to Cloud Build to run. A distro of "local"
# (the default) indicates to run the build locally, without docker, using the
# invoking user's operating system and environment.
#
# Usage: ./build.sh [options] <build-name>
#
#   Options:
#     -d|--distro <name>    The distro name to use.
#
#   Build names:

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

# Use getop to parse and normalize all the args.
readonly OPTS="d:"
readonly LONGOPTS="distro:"
PARSED="$(getopt -a \
  --options="${OPTS}" \
  --longoptions="${LONGOPTS}" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

DISTRO="local" # By default, run builds in the local environment
while true; do
  case "$1" in
  -d | --distro)
    DISTRO="$2"
    shift 2
    ;;
  --)
    shift
    break
    ;;
  esac
done
readonly DISTRO

if (($# == 0)); then
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
  basename -s .sh ci/cloudbuild/builds/*.sh | sort | xargs printf "    %s\n"
  exit 1
fi
readonly BUILD_NAME="$1"

# If a DISTRO other than "local" was requested, submit the job to Cloud Build
# to run in the specified distro.
if [ "${DISTRO}" != "local" ]; then
  # Quick checks to surface invalid arguments early
  if [ ! -r "ci/cloudbuild/Dockerfile.${DISTRO}" ]; then
    echo "Unknown distro: ${DISTRO}"
    exit 1
  elif [ ! -x "ci/cloudbuild/builds/${BUILD_NAME}.sh" ]; then
    echo "Unknown build name: ${BUILD_NAME}"
    exit 1
  fi
  io::log "====> Submitting cloud build job for ${BUILD_NAME} on ${DISTRO}"
  exec gcloud builds submit --config ci/cloudbuild/cloudbuild.yaml \
    "--substitutions=_DISTRO=${DISTRO},_BUILD_NAME=${BUILD_NAME}" \
    .
fi

io::log "====> STARTING BUILD: ${BUILD_NAME}"
readonly TIMEFORMAT="==> 🕑 ${BUILD_NAME} completed in %R seconds"
time {
  "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh"
}
