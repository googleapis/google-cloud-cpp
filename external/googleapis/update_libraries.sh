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
  ["bigquery"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/bigquery/v2:bigquery_proto" \
      "@com_google_googleapis//google/cloud/bigquery/storage/v1:storage_proto" \
      "@com_google_googleapis//google/cloud/bigquery/reservation/v1:reservation_proto" \
      "@com_google_googleapis//google/cloud/bigquery/logging/v1:logging_proto" \
      "@com_google_googleapis//google/cloud/bigquery/datatransfer/v1:datatransfer_proto" \
      "@com_google_googleapis//google/cloud/bigquery/connection/v1:connection_proto"
  )"
  ["bigtable"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/bigtable/v2:bigtable_proto" \
      "@com_google_googleapis//google/bigtable/admin/v2:admin_proto"
  )"
  ["dialogflow"]="@com_google_googleapis//google/cloud/dialogflow/v2:dialogflow_proto"
  ["iam"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/iam/credentials/v1:credentials_proto" \
      "@com_google_googleapis//google/iam/admin/v1:admin_proto"
  )"
  ["logging_type"]="@com_google_googleapis//google/logging/type:type_proto"
  ["logging"]="@com_google_googleapis//google/logging/v2:logging_proto"
  ["monitoring"]="@com_google_googleapis//google/monitoring/v3:monitoring_proto"
  ["pubsub"]="@com_google_googleapis//google/pubsub/v1:pubsub_proto"
  ["secretmanager"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/secretmanager/v1:secretmanager_proto" \
      "@com_google_googleapis//google/cloud/secretmanager/logging/v1:logging_proto"
  )"
  ["spanner"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/spanner/v1:spanner_proto" \
      "@com_google_googleapis//google/spanner/admin/instance/v1:instance_proto" \
      "@com_google_googleapis//google/spanner/admin/database/v1:database_proto"
  )"
  ["speech"]="@com_google_googleapis//google/cloud/speech/v1:speech_proto"
  ["storage"]="@com_google_googleapis//google/storage/v2:storage_proto"
  ["texttospeech"]="@com_google_googleapis//google/cloud/texttospeech/v1:texttospeech_proto"
)

for library in "${!LIBRARIES[@]}"; do
  IFS=',' read -r -a rules <<<"${LIBRARIES[$library]}"
  files=(
    "external/googleapis/protolists/${library}.list"
    "external/googleapis/protodeps/${library}.deps"
  )
  for file in "${files[@]}"; do
    : >"${file}"
  done
  for rule in "${rules[@]}"; do
    if [[ -z "${rule}" ]]; then continue; fi
    path="${rule%:*}"
    echo "=== $library $rule $path"
    bazel query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "${path}" |
      grep -E '\.proto$' \
        >>"external/googleapis/protolists/${library}.list" || true
    bazel query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "@com_google_googleapis//" | grep _proto |
      grep -v "${path}" \
        >>"external/googleapis/protodeps/${library}.deps" || true
  done
  for file in "${files[@]}"; do
    sort -u "${file}" >"${file}.sorted"
    mv "${file}.sorted" "${file}"
  done
done
