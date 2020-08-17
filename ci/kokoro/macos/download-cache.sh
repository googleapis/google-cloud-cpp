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

source "$(dirname "$0")/../../lib/init.sh"
source module lib/io.sh

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <cache-folder> <cache-name>"
  exit 1
fi
GCLOUD=gcloud
KOKORO_GFILE_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly KOKORO_GFILE_DIR

source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"
source "${PROJECT_ROOT}/ci/kokoro/cache-functions.sh"

readonly CACHE_FOLDER="$1"
readonly CACHE_NAME="$2"

mkdir -p "${HOME}/.ccache"
[[ -f "${HOME}/.ccache/ccache.conf" ]] ||
  echo "max_size = 4.0G" >"${HOME}/.ccache/ccache.conf"

if ! cache_download_enabled; then
  exit 0
fi

readonly DOWNLOAD="cmake-out/download"
mkdir -p "${DOWNLOAD}"
cache_download_tarball "${CACHE_FOLDER}" "${DOWNLOAD}" "${CACHE_NAME}.tar.gz"

echo "================================================================"
io::log "Extracting build cache"
tar -C / -zxf "${DOWNLOAD}/${CACHE_NAME}.tar.gz"
io::log "Extraction completed"

exit 0
