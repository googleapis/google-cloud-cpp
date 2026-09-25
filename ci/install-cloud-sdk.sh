#!/usr/bin/env bash
#
# Copyright 2019 Google LLC
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

set -euo pipefail

readonly GOOGLE_CLOUD_CPP_CLOUD_SDK_VERSION="474.0.0"
declare -A -r GOOGLE_CLOUD_CPP_SDK_SHA256=(
  ["x86_64"]="4af0d83c2c8d9b50fc965b314009259ccf8263c1fc8f07fd5b1bfb24f5f5bec5"
  ["arm"]="794710d1f5acdb7e6466e8879b8650fdc87e3debaaefae8f89b22929165440d4"
)

ARCH="$(uname -m)"
if [[ "${ARCH}" == "aarch64" ]]; then
  # The tarball uses this name
  ARCH="arm"
fi
readonly ARCH

components=(
  # All the emulators are in the "beta" components
  beta
  # We need the Bigtable emulator and CLI tool
  bigtable
  cbt
  # We use the Pub/Sub emulator in our tests
  pubsub-emulator
)
if [[ "${ARCH}" == "x86_64" ]]; then
  # The spanner emulator is not available for ARM64, but we do use it for testing on x86_64
  components+=(cloud-spanner-emulator)
fi

readonly SITE="https://dl.google.com/dl/cloudsdk/channels/rapid/downloads"
readonly TARBALL="google-cloud-cli-${GOOGLE_CLOUD_CPP_CLOUD_SDK_VERSION}-linux-${ARCH}.tar.gz"

curl -L "${SITE}/${TARBALL}" -o "${TARBALL}"
echo "${GOOGLE_CLOUD_CPP_SDK_SHA256[${ARCH}]} ${TARBALL}" | sha256sum --check -
tar x -C /usr/local -f "${TARBALL}"
/usr/local/google-cloud-sdk/bin/gcloud --quiet components install \
  "${components[@]}"

# vcpkg requires gsutil >= 5.35.0 for GCS binary caching, while Cloud SDK
# 474.0.0 ships with gsutil 5.27. Upgrade only platform/gsutil in-place so
# that the Cloud SDK wrapper (which passes GCE credentials) stays intact and
# emulator versions (like cloud-spanner-emulator) remain pinned to 474.0.0.
readonly GSUTIL_TARBALL="gsutil_5.35.tar.gz"
readonly GSUTIL_SHA256="8e567c0d55052f8485d646b5e6f46e82f7bc0deef061c07e4c7c0b2669a5d763"
curl -fsSL "https://storage.googleapis.com/pub/${GSUTIL_TARBALL}" -o "${GSUTIL_TARBALL}"
echo "${GSUTIL_SHA256} ${GSUTIL_TARBALL}" | sha256sum --check -
rm -rf /usr/local/google-cloud-sdk/platform/gsutil
tar -xzf "${GSUTIL_TARBALL}" -C /usr/local/google-cloud-sdk/platform
rm "${GSUTIL_TARBALL}"
