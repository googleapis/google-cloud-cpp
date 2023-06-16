#!/usr/bin/env bash
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

# Make our include guard clean against set -o nounset.
test -n "${CI_KOKORO_LIB_VCPKG_SH__:-}" || declare -i CI_KOKORO_LIB_VCPKG_SH__=0
if ((CI_KOKORO_LIB_VCPKG_SH__++ != 0)); then
  return 0
fi # include guard

source module /ci/lib/io.sh
source module /ci/kokoro/lib/cache.sh
source module /ci/kokoro/lib/gcloud.sh

install_vcpkg() {
  local -r vcpkg_dir="$1"
  local url

  mkdir -p "${vcpkg_dir}"
  io::log "Downloading vcpkg into ${vcpkg_dir}..."
  VCPKG_VERSION="$(<ci/etc/vcpkg-version.txt)"
  url="https://github.com/microsoft/vcpkg/archive/${VCPKG_VERSION}.tar.gz"
  if [[ "${VCPKG_VERSION}" =~ [0-9]{4}.[0-9]{2}.[0-9]{2} ]]; then
    # vcpkg uses date-like tags for releases
    url="https://github.com/microsoft/vcpkg/archive/refs/tags/${VCPKG_VERSION}.tar.gz"
  fi
  ci/retry-command.sh 3 120 curl -fsSL "${url}" |
    tar -C "${vcpkg_dir}" --strip-components=1 -zxf -

  io::log_h2 "Configure VCPKG to use GCS as a cache"
  readonly CACHE_BUCKET="${GOOGLE_CLOUD_CPP_KOKORO_RESULTS:-cloud-cpp-kokoro-results}"
  readonly CACHE_FOLDER="build-cache/google-cloud-cpp/vcpkg/macos"
  export VCPKG_BINARY_SOURCES="x-gcs,gs://${CACHE_BUCKET}/${CACHE_FOLDER},readwrite"
  export X_VCPKG_IGNORE_LOCK_FAILURES=true

  create_gcloud_config
  activate_service_account_keyfile "${CACHE_KEYFILE}"
  export CLOUDSDK_ACTIVE_CONFIG_NAME="${GCLOUD_CONFIG}"
  io::run gsutil ls "gs://${CACHE_BUCKET}/"
  # Eventually we can remove this, but the current caches contain both `ccache`
  # files (which we want), and `vcpkg` files (which we don't want).
  io::run rm -fr "${HOME}/.cache/vcpkg"

  ci/retry-command.sh 3 120 "${vcpkg_dir}/bootstrap-vcpkg.sh"
}
