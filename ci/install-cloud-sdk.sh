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

readonly GOOGLE_CLOUD_CPP_CLOUD_SDK_VERSION="447.0.0"
declare -A -r GOOGLE_CLOUD_CPP_SDK_SHA256=(
  ["x86_64"]="8b52fe759d24cc8b553091a412c8900a9dd3341ef9c505dbdaa395e70ae92f38"
  ["arm"]="c192b30b3ed80a58a69c604ca6b7d04a64a6c8989c2961354205fe96bd2ec6a0"
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
