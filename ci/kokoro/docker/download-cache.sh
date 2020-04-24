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

if [[ $# != 3 ]]; then
  echo "Usage: $(basename "$0") <cache-folder> <cache-name> <home-directory>"
  exit 1
fi

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(
    cd "$(dirname "$0")/../../.."
    pwd
  )"
fi
GCLOUD=gcloud
source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"
source "${PROJECT_ROOT}/ci/kokoro/cache-functions.sh"

readonly CACHE_FOLDER="$1"
readonly CACHE_NAME="$2"
readonly HOME_DIR="$3"

mkdir -p "${HOME_DIR}/.cache"
mkdir -p "${HOME_DIR}/.ccache"
echo "max_size = 4.0G" >"${HOME_DIR}/.ccache/ccache.conf"

if ! cache_download_enabled; then
  exit 0
fi

cache_download_tarball "${CACHE_FOLDER}" "${HOME_DIR}" "${CACHE_NAME}.tar.gz"

# Ignore timestamp warnings, Bazel has files with timestamps 10 years
# into the future :shrug:
echo "================================================================"
log_normal "Extracting build cache"
tar -zxf "${HOME_DIR}/${CACHE_NAME}.tar.gz" 2>&1 | grep -E -v 'tar:.*in the future'
log_normal "Extraction completed"

exit 0
