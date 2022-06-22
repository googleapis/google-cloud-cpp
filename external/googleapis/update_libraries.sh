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
  ["accessapproval"]="@com_google_googleapis//google/cloud/accessapproval/v1:accessapproval_cc_grpc"
  ["accesscontextmanager"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/identity/accesscontextmanager/type:type_cc_grpc" \
      "@com_google_googleapis//google/identity/accesscontextmanager/v1:accesscontextmanager_cc_grpc"
  )"
  ["apigateway"]="@com_google_googleapis//google/cloud/apigateway/v1:apigateway_cc_grpc"
  ["apigeeconnect"]="@com_google_googleapis//google/cloud/apigeeconnect/v1:apigeeconnect_cc_grpc"
  ["appengine"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/appengine/v1:appengine_cc_grpc" \
      "@com_google_googleapis//google/appengine/logging/v1:logging_cc_grpc" \
      "@com_google_googleapis//google/appengine/legacy:legacy_cc_grpc"
  )"
  ["artifactregistry"]="@com_google_googleapis//google/devtools/artifactregistry/v1:artifactregistry_cc_grpc"
  ["asset"]="@com_google_googleapis//google/cloud/asset/v1:asset_cc_grpc"
  ["assuredworkloads"]="@com_google_googleapis//google/cloud/assuredworkloads/v1:assuredworkloads_cc_grpc"
  ["automl"]="@com_google_googleapis//google/cloud/automl/v1:automl_cc_grpc"
  ["baremetalsolution"]="@com_google_googleapis//google/cloud/baremetalsolution/v2:baremetalsolution_cc_grpc"
  ["bigquery"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/bigquery/v2:bigquery_cc_grpc" \
      "@com_google_googleapis//google/cloud/bigquery/storage/v1:storage_cc_grpc" \
      "@com_google_googleapis//google/cloud/bigquery/reservation/v1:reservation_cc_grpc" \
      "@com_google_googleapis//google/cloud/bigquery/logging/v1:logging_cc_grpc" \
      "@com_google_googleapis//google/cloud/bigquery/datatransfer/v1:datatransfer_cc_grpc" \
      "@com_google_googleapis//google/cloud/bigquery/connection/v1:connection_cc_grpc"
  )"
  ["bigtable"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/bigtable/v2:bigtable_cc_grpc" \
      "@com_google_googleapis//google/bigtable/admin/v2:admin_cc_grpc"
  )"
  ["billing"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/billing/v1:billing_cc_grpc" \
      "@com_google_googleapis//google/cloud/billing/budgets/v1:budgets_cc_grpc"
  )"
  ["binaryauthorization"]="@com_google_googleapis//google/cloud/binaryauthorization/v1:binaryauthorization_cc_grpc"
  ["channel"]="@com_google_googleapis//google/cloud/channel/v1:channel_cc_grpc"
  ["cloudbuild"]="@com_google_googleapis//google/devtools/cloudbuild/v1:cloudbuild_cc_grpc"
  ["common"]="@com_google_googleapis//google/cloud/common:common_cc_grpc"
  ["composer"]="@com_google_googleapis//google/cloud/orchestration/airflow/service/v1:service_cc_grpc"
  ["contactcenterinsights"]="@com_google_googleapis//google/cloud/contactcenterinsights/v1:contactcenterinsights_cc_grpc"
  ["container"]="@com_google_googleapis//google/container/v1:container_cc_grpc"
  ["containeranalysis"]="@com_google_googleapis//google/devtools/containeranalysis/v1:containeranalysis_cc_grpc"
  ["datacatalog"]="@com_google_googleapis//google/cloud/datacatalog/v1:datacatalog_cc_grpc"
  ["datamigration"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/clouddms/v1:clouddms_cc_grpc" \
      "@com_google_googleapis//google/cloud/clouddms/logging/v1:logging_cc_grpc"
  )"
  ["dataplex"]="@com_google_googleapis//google/cloud/dataplex/v1:dataplex_cc_grpc"
  ["dataproc"]="@com_google_googleapis//google/cloud/dataproc/v1:dataproc_cc_grpc"
  ["debugger"]="@com_google_googleapis//google/devtools/clouddebugger/v2:clouddebugger_cc_grpc"
  ["dialogflow"]="@com_google_googleapis//google/cloud/dialogflow/v2:dialogflow_cc_grpc"
  ["dialogflow_cx"]="@com_google_googleapis//google/cloud/dialogflow/cx/v3:cx_cc_grpc"
  ["dlp"]="@com_google_googleapis//google/privacy/dlp/v2:dlp_cc_grpc"
  ["documentai"]="@com_google_googleapis//google/cloud/documentai/v1:documentai_cc_grpc"
  ["eventarc"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/eventarc/v1:eventarc_cc_grpc" \
      "@com_google_googleapis//google/cloud/eventarc/publishing/v1:publishing_cc_grpc"
  )"
  ["filestore"]="@com_google_googleapis//google/cloud/filestore/v1:filestore_cc_grpc"
  ["functions"]="@com_google_googleapis//google/cloud/functions/v1:functions_cc_grpc"
  ["gameservices"]="@com_google_googleapis//google/cloud/gaming/v1:gaming_cc_grpc"
  ["gkehub"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/gkehub/v1:gkehub_cc_grpc" \
      "@com_google_googleapis//google/cloud/gkehub/v1/multiclusteringress:multiclusteringress_cc_grpc" \
      "@com_google_googleapis//google/cloud/gkehub/v1/configmanagement:configmanagement_cc_grpc"
  )"
  ["grafeas"]="@com_google_googleapis//grafeas/v1:grafeas_cc_grpc"
  ["iam"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/iam/credentials/v1:credentials_cc_grpc" \
      "@com_google_googleapis//google/iam/admin/v1:admin_cc_grpc"
  )"
  # We already have libraries, different from `iam_protos` that compile
  # the protos defined in the following entry. Thus, we cannot merge
  # the following entry into the existing `iam` entry, that would
  # introduce either ODR-violations, or break backwards compatibility.
  ["iam_policy"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/iam/v1:iam_cc_grpc" \
      "@com_google_googleapis//google/iam/v1/logging:logging_cc_grpc"
  )"
  ["iap"]="@com_google_googleapis//google/cloud/iap/v1:iap_cc_grpc"
  ["ids"]="@com_google_googleapis//google/cloud/ids/v1:ids_cc_grpc"
  ["iot"]="@com_google_googleapis//google/cloud/iot/v1:iot_cc_grpc"
  ["kms"]="@com_google_googleapis//google/cloud/kms/v1:kms_cc_grpc"
  ["language"]="@com_google_googleapis//google/cloud/language/v1:language_cc_grpc"
  ["logging_type"]="@com_google_googleapis//google/logging/type:type_cc_grpc"
  ["logging"]="@com_google_googleapis//google/logging/v2:logging_cc_grpc"
  ["managedidentities"]="@com_google_googleapis//google/cloud/managedidentities/v1:managedidentities_cc_grpc"
  ["memcache"]="@com_google_googleapis//google/cloud/memcache/v1:memcache_cc_grpc"
  ["monitoring"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/monitoring/v3:monitoring_cc_grpc" \
      "@com_google_googleapis//google/monitoring/dashboard/v1:dashboard_cc_grpc" \
      "@com_google_googleapis//google/monitoring/metricsscope/v1:metricsscope_cc_grpc"
  )"
  ["networkmanagement"]="@com_google_googleapis//google/cloud/networkmanagement/v1:networkmanagement_cc_grpc"
  ["notebooks"]="@com_google_googleapis//google/cloud/notebooks/v1:notebooks_cc_grpc"
  ["optimization"]="@com_google_googleapis//google/cloud/optimization/v1:optimization_cc_grpc"
  ["orgpolicy"]="@com_google_googleapis//google/cloud/orgpolicy/v2:orgpolicy_cc_grpc"
  ["osconfig"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/osconfig/agentendpoint/v1:agentendpoint_cc_grpc" \
      "@com_google_googleapis//google/cloud/osconfig/v1:osconfig_cc_grpc"
  )"
  ["oslogin"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/oslogin/v1:oslogin_cc_grpc" \
      "@com_google_googleapis//google/cloud/oslogin/common:common_cc_grpc"
  )"
  ["policytroubleshooter"]="@com_google_googleapis//google/cloud/policytroubleshooter/v1:policytroubleshooter_cc_grpc"
  ["privateca"]="@com_google_googleapis//google/cloud/security/privateca/v1:privateca_cc_grpc"
  ["profiler"]="@com_google_googleapis//google/devtools/cloudprofiler/v2:cloudprofiler_cc_grpc"
  ["pubsub"]="@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc"
  ["pubsublite"]="@com_google_googleapis//google/cloud/pubsublite/v1:pubsublite_cc_grpc"
  ["recommender"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/recommender/v1:recommender_cc_grpc" \
      "@com_google_googleapis//google/cloud/recommender/logging/v1:logging_cc_grpc"
  )"
  ["redis"]="@com_google_googleapis//google/cloud/redis/v1:redis_cc_grpc"
  ["resourcemanager"]="@com_google_googleapis//google/cloud/resourcemanager/v3:resourcemanager_cc_grpc"
  ["resourcesettings"]="@com_google_googleapis//google/cloud/resourcesettings/v1:resourcesettings_cc_grpc"
  ["retail"]="@com_google_googleapis//google/cloud/retail/v2:retail_cc_grpc"
  ["scheduler"]="@com_google_googleapis//google/cloud/scheduler/v1:scheduler_cc_grpc"
  ["secretmanager"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/secretmanager/v1:secretmanager_cc_grpc" \
      "@com_google_googleapis//google/cloud/secretmanager/logging/v1:logging_cc_grpc"
  )"
  ["securitycenter"]="@com_google_googleapis//google/cloud/securitycenter/v1:securitycenter_cc_grpc"
  ["servicecontrol"]="@com_google_googleapis//google/api/servicecontrol/v1:servicecontrol_cc_grpc"
  ["servicedirectory"]="@com_google_googleapis//google/cloud/servicedirectory/v1:servicedirectory_cc_grpc"
  ["servicemanagement"]="@com_google_googleapis//google/api/servicemanagement/v1:servicemanagement_cc_grpc"
  ["serviceusage"]="@com_google_googleapis//google/api/serviceusage/v1:serviceusage_cc_grpc"
  ["shell"]="@com_google_googleapis//google/cloud/shell/v1:shell_cc_grpc"
  ["spanner"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/spanner/v1:spanner_cc_grpc" \
      "@com_google_googleapis//google/spanner/admin/instance/v1:instance_cc_grpc" \
      "@com_google_googleapis//google/spanner/admin/database/v1:database_cc_grpc"
  )"
  ["speech"]="@com_google_googleapis//google/cloud/speech/v1:speech_cc_grpc"
  ["storage"]="@com_google_googleapis//google/storage/v2:storage_cc_grpc"
  ["storagetransfer"]="@com_google_googleapis//google/storagetransfer/v1:storagetransfer_cc_grpc"
  ["talent"]="@com_google_googleapis//google/cloud/talent/v4:talent_cc_grpc"
  ["tasks"]="@com_google_googleapis//google/cloud/tasks/v2:tasks_cc_grpc"
  ["texttospeech"]="@com_google_googleapis//google/cloud/texttospeech/v1:texttospeech_cc_grpc"
  ["tpu"]="@com_google_googleapis//google/cloud/tpu/v1:tpu_cc_grpc"
  ["trace"]="@com_google_googleapis//google/devtools/cloudtrace/v2:cloudtrace_cc_grpc"
  ["translate"]="@com_google_googleapis//google/cloud/translate/v3:translation_cc_grpc"
  ["video"]="$(
    printf ",%s" \
    "@com_google_googleapis//google/cloud/video/stitcher/v1:stitcher_cc_grpc" \
    "@com_google_googleapis//google/cloud/video/transcoder/v1:transcoder_cc_grpc"
  )"
  ["videointelligence"]="@com_google_googleapis//google/cloud/videointelligence/v1:videointelligence_cc_grpc"
  ["vision"]="@com_google_googleapis//google/cloud/vision/v1:vision_cc_grpc"
  ["vmmigration"]="@com_google_googleapis//google/cloud/vmmigration/v1:vmmigration_cc_grpc"
  ["vpcaccess"]="@com_google_googleapis//google/cloud/vpcaccess/v1:vpcaccess_cc_grpc"
  ["webrisk"]="@com_google_googleapis//google/cloud/webrisk/v1:webrisk_cc_grpc"
  ["websecurityscanner"]="@com_google_googleapis//google/cloud/websecurityscanner/v1:websecurityscanner_cc_grpc"
  ["workflows"]="$(
    printf ",%s" \
      "@com_google_googleapis//google/cloud/workflows/v1:workflows_cc_grpc" \
      "@com_google_googleapis//google/cloud/workflows/type:type_cc_grpc" \
      "@com_google_googleapis//google/cloud/workflows/executions/v1:executions_cc_grpc"
  )"
)

if [[ $# -eq 0 ]]; then
  keys=("${!LIBRARIES[@]}")
else
  keys=("$@")
fi

for library in "${keys[@]}"; do
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
    LC_ALL=C sort -u "${file}" >"${file}.sorted"
    mv "${file}.sorted" "${file}"
  done
done
