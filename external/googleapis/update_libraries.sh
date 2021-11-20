#!/usr/bin/env bash
# Copyright 2021 Google LLC
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

declare -A -r LIBRARIES=(
  ["texttospeech"]="@com_google_googleapis//google/cloud/texttospeech/v1:texttospeech_proto"
)

for library in "${!LIBRARIES[@]}"; do
  IFS=',' read -r -a rules <<<"${LIBRARIES[$library]}"
  : >"external/googleapis/protolists/${library}.list"
  : >"external/googleapis/protodeps/${library}.deps"
  for rule in "${rules[@]}"; do
    path="${rule%:*}"
    bazel --batch query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "${path}" |
      grep -E '\.proto$' \
        >>"external/googleapis/protolists/${library}.list"
    bazel --batch query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "@com_google_googleapis//" | grep _proto |
      grep -v "${path}" \
        >>"external/googleapis/protodeps/${library}.deps"
  done
done
