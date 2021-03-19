#!/bin/bash
#
# Copyright 2021 Google Inc.
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
# scripts in the `builds/` directory. If this script is invoked from a user at
# a terminal it submits the Cloud Build job on the user's behalf. If the script
# is invoked from within a Cloud Build environment (determined by checking
# ${BUILD_ID}), then it'll kick off the named build.
#
# Usage: ./build.sh <build-name>

set -eu

source $(dirname $0)/../lib/init.sh
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

# Extracts the usage from the file comment starting at line 17.
readonly USAGE="$(sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}")"
if (( $# == 0 )); then
  echo "${USAGE}"
  printf "\n  Valid build names:\n"
  printf "    %s\n" $(basename -s .sh ci/cloudbuild/builds/*.sh | sort)
  exit 0
fi

readonly BUILD_NAME="$1"

# If BUILD_ID is unset, assume this script was invoked by a user and submit the
# cloud build job that was requsted.
if [ -z "${BUILD_ID:-}" ]; then
  io::log "====> Submitting cloud build job for ${BUILD_NAME}"
  exec gcloud builds submit --config ci/cloudbuild/cloudbuild.yaml \
    --substitutions=_DISTRO=fedora,_BUILD_NAME="${BUILD_NAME}" .
fi

io::log "====> STARTING BUILD: ${BUILD_NAME}"

readonly TIMEFORMAT="==> 🕑 ${BUILD_NAME} completed in %R seconds"
time {
  "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh"
}
