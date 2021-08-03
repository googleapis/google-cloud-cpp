#!/usr/bin/env bash
#
# Copyright 2019 Google LLC
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

set -euo pipefail

readonly GOOGLE_CLOUD_CPP_CLOUD_SDK_VERSION="348.0.0"
readonly GOOGLE_CLOUD_CPP_SDK_SHA256="8341a9b21088fd382522be247c7e51c61d8ea4ff86e6ededfa601afd5223e153"

readonly SITE="https://dl.google.com/dl/cloudsdk/channels/rapid/downloads"
readonly TARBALL="google-cloud-sdk-${GOOGLE_CLOUD_CPP_CLOUD_SDK_VERSION}-linux-x86_64.tar.gz"
curl -L "${SITE}/${TARBALL}" -o "${TARBALL}"

echo "${GOOGLE_CLOUD_CPP_SDK_SHA256} ${TARBALL}" | sha256sum --check -
tar x -C /usr/local -f "${TARBALL}"
/usr/local/google-cloud-sdk/bin/gcloud --quiet components install \
  beta bigtable cbt cloud-spanner-emulator pubsub-emulator
