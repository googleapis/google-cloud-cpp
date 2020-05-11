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
source module etc/integration-tests-config.sh
source module etc/quickstart-config.sh
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

if ! cache_upload_enabled; then
  exit 0
fi

maybe_dirs=(
  # This is where ccache stores its files, if present we want to back it up
  "${HOME}/.ccache"

  # The quickstart builds need to preserve the contents of the vcpkg installed/
  # directory, but we have to separate it from the other files in `vcpkg` or
  # things like `git clone` do not work as expected.
  "${PROJECT_ROOT}/cmake-out/upload/vcpkg-installed"
)

readonly BAZEL_BIN="$HOME/bin/bazel"
if [[ -x "${BAZEL_BIN}" ]]; then
  maybe_dirs+=("$("${BAZEL_BIN}" info repository_cache)")
  maybe_dirs+=("$("${BAZEL_BIN}" info output_base)")
  "${BAZEL_BIN}" shutdown

  for library in $(quickstart::libraries); do
    cd "${PROJECT_ROOT}/google/cloud/${library}/quickstart"
    maybe_dirs+=("$("${BAZEL_BIN}" info repository_cache)")
    maybe_dirs+=("$("${BAZEL_BIN}" info output_base)")
    "${BAZEL_BIN}" shutdown
  done
  cd "${PROJECT_ROOT}"
fi

dirs=()
for dir in "${maybe_dirs[@]}"; do
  if [[ -d "${dir}" ]]; then dirs+=("${dir}"); fi
done

readonly UPLOAD="cmake-out/upload"
mkdir -p "${UPLOAD}"

echo "================================================================"
io::log "Preparing cache tarball for ${CACHE_NAME}"
tar -C / -zcf "${UPLOAD}/${CACHE_NAME}.tar.gz" "${dirs[@]}"

cache_upload_tarball "${UPLOAD}" "${CACHE_NAME}.tar.gz" "${CACHE_FOLDER}"

exit 0
