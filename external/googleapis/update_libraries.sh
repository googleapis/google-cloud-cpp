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
  ["accessapproval"]="@googleapis//google/cloud/accessapproval/v1:accessapproval_cc_grpc"
  ["accesscontextmanager"]="$(
    printf ",%s" \
      "@googleapis//google/identity/accesscontextmanager/type:type_cc_grpc" \
      "@googleapis//google/identity/accesscontextmanager/v1:accesscontextmanager_cc_grpc"
  )"
  ["advisorynotifications"]="@googleapis//google/cloud/advisorynotifications/v1:advisorynotifications_cc_grpc"
  ["aiplatform"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/aiplatform/logging:logging_cc_grpc" \
      "@googleapis//google/cloud/aiplatform/v1:aiplatform_cc_grpc" \
      "@googleapis//google/cloud/aiplatform/v1/schema/predict/instance:instance_cc_grpc" \
      "@googleapis//google/cloud/aiplatform/v1/schema/predict/params:params_cc_grpc" \
      "@googleapis//google/cloud/aiplatform/v1/schema/predict/prediction:prediction_cc_grpc" \
      "@googleapis//google/cloud/aiplatform/v1/schema/trainingjob/definition:definition_cc_grpc"
  )"
  ["alloydb"]="@googleapis//google/cloud/alloydb/v1:alloydb_cc_grpc"
  ["apigateway"]="@googleapis//google/cloud/apigateway/v1:apigateway_cc_grpc"
  ["apigeeconnect"]="@googleapis//google/cloud/apigeeconnect/v1:apigeeconnect_cc_grpc"
  ["apikeys"]="@googleapis//google/api/apikeys/v2:apikeys_cc_grpc"
  ["appengine"]="$(
    printf ",%s" \
      "@googleapis//google/appengine/v1:appengine_cc_grpc" \
      "@googleapis//google/appengine/logging/v1:logging_cc_grpc" \
      "@googleapis//google/appengine/legacy:legacy_cc_grpc"
  )"
  ["apphub"]="@googleapis//google/cloud/apphub/v1:apphub_cc_grpc"
  ["artifactregistry"]="@googleapis//google/devtools/artifactregistry/v1:artifactregistry_cc_grpc"
  ["asset"]="@googleapis//google/cloud/asset/v1:asset_cc_grpc"
  ["assuredworkloads"]="@googleapis//google/cloud/assuredworkloads/v1:assuredworkloads_cc_grpc"
  ["automl"]="@googleapis//google/cloud/automl/v1:automl_cc_grpc"
  ["backupdr"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/backupdr/v1:backupdr_cc_grpc" \
      "@googleapis//google/cloud/backupdr/logging/v1:logging_cc_grpc"
  )"
  ["baremetalsolution"]="@googleapis//google/cloud/baremetalsolution/v2:baremetalsolution_cc_grpc"
  ["batch"]="@googleapis//google/cloud/batch/v1:batch_cc_grpc"
  ["beyondcorp"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/beyondcorp/appconnections/v1:appconnections_cc_grpc" \
      "@googleapis//google/cloud/beyondcorp/appconnectors/v1:appconnectors_cc_grpc" \
      "@googleapis//google/cloud/beyondcorp/appgateways/v1:appgateways_cc_grpc"
  )"
  ["bigquery"]="$(
    # This is long enough that it needs to be kept in alphabetical order
    printf ",%s" \
      "@googleapis//google/cloud/bigquery/analyticshub/v1:analyticshub_cc_grpc" \
      "@googleapis//google/cloud/bigquery/biglake/v1:biglake_cc_grpc" \
      "@googleapis//google/cloud/bigquery/connection/v1:connection_cc_grpc" \
      "@googleapis//google/cloud/bigquery/datapolicies/v1:datapolicies_cc_grpc" \
      "@googleapis//google/cloud/bigquery/datapolicies/v2:datapolicies_cc_grpc" \
      "@googleapis//google/cloud/bigquery/datatransfer/v1:datatransfer_cc_grpc" \
      "@googleapis//google/cloud/bigquery/logging/v1:logging_cc_grpc" \
      "@googleapis//google/cloud/bigquery/migration/v2:migration_cc_grpc" \
      "@googleapis//google/cloud/bigquery/reservation/v1:reservation_cc_grpc" \
      "@googleapis//google/cloud/bigquery/storage/v1:storage_cc_grpc"
  )"
  ["bigquerycontrol"]="@googleapis//google/cloud/bigquery/v2:bigquery_cc_proto"
  ["bigtable"]="$(
    printf ",%s" \
      "@googleapis//google/bigtable/v2:bigtable_cc_grpc" \
      "@googleapis//google/bigtable/admin/v2:admin_cc_grpc"
  )"
  ["billing"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/billing/v1:billing_cc_grpc" \
      "@googleapis//google/cloud/billing/budgets/v1:budgets_cc_grpc"
  )"
  ["binaryauthorization"]="@googleapis//google/cloud/binaryauthorization/v1:binaryauthorization_cc_grpc"
  ["certificatemanager"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/certificatemanager/logging/v1:logging_cc_grpc" \
      "@googleapis//google/cloud/certificatemanager/v1:certificatemanager_cc_grpc"
  )"
  ["channel"]="@googleapis//google/cloud/channel/v1:channel_cc_grpc"
  ["chronicle"]="@googleapis//google/cloud/chronicle/v1:chronicle_cc_grpc"
  ["cloudbuild"]="$(
    printf ",%s" \
      "@googleapis//google/devtools/cloudbuild/v1:cloudbuild_cc_grpc" \
      "@googleapis//google/devtools/cloudbuild/v2:cloudbuild_cc_grpc"
  )"
  ["cloudcontrolspartner"]="@googleapis//google/cloud/cloudcontrolspartner/v1:cloudcontrolspartner_cc_grpc"
  ["cloudquotas"]="@googleapis//google/api/cloudquotas/v1:cloudquotas_cc_grpc"
  ["commerce"]="@googleapis//google/cloud/commerce/consumer/procurement/v1:procurement_cc_grpc"
  ["common"]="@googleapis//google/cloud/common:common_cc_grpc"
  ["composer"]="@googleapis//google/cloud/orchestration/airflow/service/v1:service_cc_grpc"
  ["confidentialcomputing"]="@googleapis//google/cloud/confidentialcomputing/v1:confidentialcomputing_cc_grpc"
  ["config"]="@googleapis//google/cloud/config/v1:config_cc_grpc"
  ["connectors"]="@googleapis//google/cloud/connectors/v1:connectors_cc_grpc"
  ["contactcenterinsights"]="@googleapis//google/cloud/contactcenterinsights/v1:contactcenterinsights_cc_grpc"
  ["container"]="@googleapis//google/container/v1:container_cc_grpc"
  ["containeranalysis"]="@googleapis//google/devtools/containeranalysis/v1:containeranalysis_cc_grpc"
  ["contentwarehouse"]="@googleapis//google/cloud/contentwarehouse/v1:contentwarehouse_cc_grpc"
  ["datacatalog"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/datacatalog/v1:datacatalog_cc_grpc" \
      "@googleapis//google/cloud/datacatalog/lineage/v1:lineage_cc_grpc"
  )"
  ["dataform"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/dataform/v1:dataform_cc_grpc" \
      "@googleapis//google/cloud/dataform/logging/v1:logging_cc_grpc"
  )"
  ["datafusion"]="@googleapis//google/cloud/datafusion/v1:datafusion_cc_grpc"
  ["datamigration"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/clouddms/v1:clouddms_cc_grpc" \
      "@googleapis//google/cloud/clouddms/logging/v1:logging_cc_grpc"
  )"
  ["dataplex"]="@googleapis//google/cloud/dataplex/v1:dataplex_cc_grpc"
  ["dataproc"]="@googleapis//google/cloud/dataproc/v1:dataproc_cc_grpc"
  ["datastore"]="$(
    printf ",%s" \
      "@googleapis//google/datastore/admin/v1:admin_cc_grpc" \
      "@googleapis//google/datastore/v1:datastore_cc_grpc"
  )"
  ["datastream"]="@googleapis//google/cloud/datastream/v1:datastream_cc_grpc"
  ["deploy"]="@googleapis//google/cloud/deploy/v1:deploy_cc_grpc"
  ["developerconnect"]="@googleapis//google/cloud/developerconnect/v1:developerconnect_cc_grpc"
  ["devicestreaming"]="@googleapis//google/cloud/devicestreaming/v1:devicestreaming_cc_grpc"
  ["dialogflow_es"]="@googleapis//google/cloud/dialogflow/v2:dialogflow_cc_grpc"
  ["dialogflow_cx"]="@googleapis//google/cloud/dialogflow/cx/v3:cx_cc_grpc"
  ["discoveryengine"]="@googleapis//google/cloud/discoveryengine/v1:discoveryengine_cc_grpc"
  ["dlp"]="@googleapis//google/privacy/dlp/v2:dlp_cc_grpc"
  ["documentai"]="@googleapis//google/cloud/documentai/v1:documentai_cc_grpc"
  ["domains"]="@googleapis//google/cloud/domains/v1:domains_cc_grpc"
  ["edgecontainer"]="@googleapis//google/cloud/edgecontainer/v1:edgecontainer_cc_grpc"
  ["edgenetwork"]="@googleapis//google/cloud/edgenetwork/v1:edgenetwork_cc_grpc"
  ["essentialcontacts"]="@googleapis//google/cloud/essentialcontacts/v1:essentialcontacts_cc_grpc"
  ["eventarc"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/eventarc/v1:eventarc_cc_grpc" \
      "@googleapis//google/cloud/eventarc/publishing/v1:publishing_cc_grpc"
  )"
  ["filestore"]="@googleapis//google/cloud/filestore/v1:filestore_cc_grpc"
  ["financialservices"]="@googleapis//google/cloud/financialservices/v1:financialservices_cc_grpc"
  ["functions"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/functions/v1:functions_cc_grpc" \
      "@googleapis//google/cloud/functions/v2:functions_cc_grpc"
  )"
  ["gkebackup"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/gkebackup/logging/v1:logging_cc_grpc" \
      "@googleapis//google/cloud/gkebackup/v1:gkebackup_cc_grpc"
  )"
  ["gkeconnect"]="@googleapis//google/cloud/gkeconnect/gateway/v1:gateway_cc_grpc"
  ["gkehub"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/gkehub/v1:gkehub_cc_grpc" \
      "@googleapis//google/cloud/gkehub/v1/multiclusteringress:multiclusteringress_cc_grpc" \
      "@googleapis//google/cloud/gkehub/v1/configmanagement:configmanagement_cc_grpc"
  )"
  ["gkemulticloud"]="@googleapis//google/cloud/gkemulticloud/v1:gkemulticloud_cc_grpc"
  ["grafeas"]="@googleapis//grafeas/v1:grafeas_cc_grpc"
  ["iam"]="@googleapis//google/iam/admin/v1:admin_cc_grpc"
  ["iam_v2"]="@googleapis//google/iam/v2:iam_cc_grpc"
  ["iam_v3"]="@googleapis//google/iam/v3:iam_cc_grpc"
  ["iap"]="@googleapis//google/cloud/iap/v1:iap_cc_grpc"
  ["ids"]="@googleapis//google/cloud/ids/v1:ids_cc_grpc"
  ["kms"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/kms/v1:kms_cc_grpc" \
      "@googleapis//google/cloud/kms/inventory/v1:inventory_cc_grpc"
  )"
  ["language"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/language/v1:language_cc_grpc" \
      "@googleapis//google/cloud/language/v2:language_cc_grpc"
  )"
  ["licensemanager"]="@googleapis//google/cloud/licensemanager/v1:licensemanager_cc_grpc"
  ["logging_type"]="@googleapis//google/logging/type:type_cc_grpc"
  ["logging"]="@googleapis//google/logging/v2:logging_cc_grpc"
  ["lustre"]="@googleapis//google/cloud/lustre/v1:lustre_cc_grpc"
  ["managedidentities"]="@googleapis//google/cloud/managedidentities/v1:managedidentities_cc_grpc"
  ["managedkafka"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/managedkafka/schemaregistry/v1:schemaregistry_cc_grpc" \
      "@googleapis//google/cloud/managedkafka/v1:managedkafka_cc_grpc"
  )"
  ["memcache"]="@googleapis//google/cloud/memcache/v1:memcache_cc_grpc"
  ["memorystore"]="@googleapis//google/cloud/memorystore/v1:memorystore_cc_grpc"
  ["metastore"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/metastore/logging/v1:logging_cc_grpc" \
      "@googleapis//google/cloud/metastore/v1:metastore_cc_grpc"
  )"
  ["migrationcenter"]="@googleapis//google/cloud/migrationcenter/v1:migrationcenter_cc_grpc"
  ["monitoring"]="$(
    printf ",%s" \
      "@googleapis//google/monitoring/v3:monitoring_cc_grpc" \
      "@googleapis//google/monitoring/dashboard/v1:dashboard_cc_grpc" \
      "@googleapis//google/monitoring/metricsscope/v1:metricsscope_cc_grpc"
  )"
  ["netapp"]="@googleapis//google/cloud/netapp/v1:netapp_cc_grpc"
  ["networkconnectivity"]="@googleapis//google/cloud/networkconnectivity/v1:networkconnectivity_cc_grpc"
  ["networkmanagement"]="@googleapis//google/cloud/networkmanagement/v1:networkmanagement_cc_grpc"
  ["networksecurity"]="@googleapis//google/cloud/networksecurity/v1:networksecurity_cc_grpc"
  ["networkservices"]="@googleapis//google/cloud/networkservices/v1:networkservices_cc_grpc"
  ["notebooks"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/notebooks/v1:notebooks_cc_grpc" \
      "@googleapis//google/cloud/notebooks/v2:notebooks_cc_grpc"
  )"
  ["optimization"]="@googleapis//google/cloud/optimization/v1:optimization_cc_grpc"
  ["oracledatabase"]="@googleapis//google/cloud/oracledatabase/v1:oracledatabase_cc_grpc"
  ["orgpolicy"]="@googleapis//google/cloud/orgpolicy/v2:orgpolicy_cc_grpc"
  ["osconfig"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/osconfig/agentendpoint/v1:agentendpoint_cc_grpc" \
      "@googleapis//google/cloud/osconfig/v1:osconfig_cc_grpc"
  )"
  ["oslogin"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/oslogin/v1:oslogin_cc_grpc" \
      "@googleapis//google/cloud/oslogin/common:common_cc_grpc"
  )"
  ["parametermanager"]="@googleapis//google/cloud/parametermanager/v1:parametermanager_cc_grpc"
  ["parallelstore"]="@googleapis//google/cloud/parallelstore/v1:parallelstore_cc_grpc"
  ["policysimulator"]="@googleapis//google/cloud/policysimulator/v1:policysimulator_cc_grpc"
  ["policytroubleshooter"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/policytroubleshooter/v1:policytroubleshooter_cc_grpc" \
      "@googleapis//google/cloud/policytroubleshooter/iam/v3:iam_cc_grpc"
  )"
  ["privateca"]="@googleapis//google/cloud/security/privateca/v1:privateca_cc_grpc"
  ["privilegedaccessmanager"]="@googleapis//google/cloud/privilegedaccessmanager/v1:privilegedaccessmanager_cc_grpc"
  ["profiler"]="@googleapis//google/devtools/cloudprofiler/v2:cloudprofiler_cc_grpc"
  ["publicca"]="@googleapis//google/cloud/security/publicca/v1:publicca_cc_grpc"
  ["pubsub"]="@googleapis//google/pubsub/v1:pubsub_cc_grpc"
  ["pubsublite"]="@googleapis//google/cloud/pubsublite/v1:pubsublite_cc_grpc"
  ["rapidmigrationassessment"]="@googleapis//google/cloud/rapidmigrationassessment/v1:rapidmigrationassessment_cc_grpc"
  ["recaptchaenterprise"]="@googleapis//google/cloud/recaptchaenterprise/v1:recaptchaenterprise_cc_grpc"
  ["recommender"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/recommender/v1:recommender_cc_grpc" \
      "@googleapis//google/cloud/recommender/logging/v1:logging_cc_grpc"
  )"
  ["redis"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/redis/cluster/v1:cluster_cc_grpc" \
      "@googleapis//google/cloud/redis/v1:redis_cc_grpc"
  )"
  ["resourcemanager"]="@googleapis//google/cloud/resourcemanager/v3:resourcemanager_cc_grpc"
  ["retail"]="@googleapis//google/cloud/retail/v2:retail_cc_grpc"
  ["run"]="@googleapis//google/cloud/run/v2:run_cc_grpc"
  ["scheduler"]="@googleapis//google/cloud/scheduler/v1:scheduler_cc_grpc"
  ["secretmanager"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/secretmanager/v1:secretmanager_cc_grpc" \
      "@googleapis//google/cloud/secretmanager/logging/v1:logging_cc_grpc"
  )"
  ["securesourcemanager"]="@googleapis//google/cloud/securesourcemanager/v1:securesourcemanager_cc_grpc"
  ["securitycenter"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/securitycenter/v1:securitycenter_cc_grpc" \
      "@googleapis//google/cloud/securitycenter/v2:securitycenter_cc_grpc"
  )"
  ["securitycentermanagement"]="@googleapis//google/cloud/securitycentermanagement/v1:securitycentermanagement_cc_grpc"
  ["servicecontrol"]="$(
    printf ",%s" \
      "@googleapis//google/api/servicecontrol/v1:servicecontrol_cc_grpc" \
      "@googleapis//google/api/servicecontrol/v2:servicecontrol_cc_grpc"
  )"
  ["servicedirectory"]="@googleapis//google/cloud/servicedirectory/v1:servicedirectory_cc_grpc"
  ["servicehealth"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/servicehealth/v1:servicehealth_cc_grpc" \
      "@googleapis//google/cloud/servicehealth/logging/v1:logging_cc_grpc"
  )"
  ["servicemanagement"]="@googleapis//google/api/servicemanagement/v1:servicemanagement_cc_grpc"
  ["serviceusage"]="@googleapis//google/api/serviceusage/v1:serviceusage_cc_grpc"
  ["shell"]="@googleapis//google/cloud/shell/v1:shell_cc_grpc"
  ["spanner"]="$(
    printf ",%s" \
      "@googleapis//google/spanner/v1:spanner_cc_grpc" \
      "@googleapis//google/spanner/admin/instance/v1:instance_cc_grpc" \
      "@googleapis//google/spanner/admin/database/v1:database_cc_grpc"
  )"
  ["speech"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/speech/v1:speech_cc_grpc" \
      "@googleapis//google/cloud/speech/v2:speech_cc_grpc"
  )"
  ["sql"]="@googleapis//google/cloud/sql/v1:sql_cc_proto"
  ["storage"]="@googleapis//google/storage/v2:storage_cc_grpc"
  ["storagebatchoperations"]="@googleapis//google/cloud/storagebatchoperations/v1:storagebatchoperations_cc_grpc"
  ["storagecontrol"]="@googleapis//google/storage/control/v2:control_cc_grpc"
  ["storageinsights"]="@googleapis//google/cloud/storageinsights/v1:storageinsights_cc_grpc"
  ["storagetransfer"]="@googleapis//google/storagetransfer/v1:storagetransfer_cc_grpc"
  ["support"]="@googleapis//google/cloud/support/v2:support_cc_grpc"
  ["talent"]="@googleapis//google/cloud/talent/v4:talent_cc_grpc"
  ["tasks"]="@googleapis//google/cloud/tasks/v2:tasks_cc_grpc"
  ["telcoautomation"]="@googleapis//google/cloud/telcoautomation/v1:telcoautomation_cc_grpc"
  ["texttospeech"]="@googleapis//google/cloud/texttospeech/v1:texttospeech_cc_grpc"
  ["timeseriesinsights"]="@googleapis//google/cloud/timeseriesinsights/v1:timeseriesinsights_cc_grpc"
  ["tpu"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/tpu/v1:tpu_cc_grpc" \
      "@googleapis//google/cloud/tpu/v2:tpu_cc_grpc"
  )"
  ["trace"]="$(
    printf ",%s" \
      "@googleapis//google/devtools/cloudtrace/v1:cloudtrace_cc_grpc" \
      "@googleapis//google/devtools/cloudtrace/v2:cloudtrace_cc_grpc"
  )"
  ["translate"]="@googleapis//google/cloud/translate/v3:translation_cc_grpc"
  ["video"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/video/livestream/v1:livestream_cc_grpc" \
      "@googleapis//google/cloud/video/stitcher/v1:stitcher_cc_grpc" \
      "@googleapis//google/cloud/video/transcoder/v1:transcoder_cc_grpc"
  )"
  ["videointelligence"]="@googleapis//google/cloud/videointelligence/v1:videointelligence_cc_grpc"
  ["vision"]="@googleapis//google/cloud/vision/v1:vision_cc_grpc"
  ["vmmigration"]="@googleapis//google/cloud/vmmigration/v1:vmmigration_cc_grpc"
  ["vmwareengine"]="@googleapis//google/cloud/vmwareengine/v1:vmwareengine_cc_grpc"
  ["vpcaccess"]="@googleapis//google/cloud/vpcaccess/v1:vpcaccess_cc_grpc"
  ["webrisk"]="@googleapis//google/cloud/webrisk/v1:webrisk_cc_grpc"
  ["websecurityscanner"]="@googleapis//google/cloud/websecurityscanner/v1:websecurityscanner_cc_grpc"
  ["workflows"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/workflows/v1:workflows_cc_grpc" \
      "@googleapis//google/cloud/workflows/type:type_cc_grpc" \
      "@googleapis//google/cloud/workflows/executions/v1:executions_cc_grpc"
  )"
  ["workstations"]="$(
    printf ",%s" \
      "@googleapis//google/cloud/workstations/logging/v1:logging_cc_grpc" \
      "@googleapis//google/cloud/workstations/v1:workstations_cc_grpc"
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
    bazelisk query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "${path}" |
      grep -E '\.proto$' \
        >>"external/googleapis/protolists/${library}.list" || true
    bazelisk query --noshow_progress --noshow_loading_progress \
      "deps(${rule})" |
      grep "@googleapis//" | grep _proto |
      grep -v "${path}" \
        >>"external/googleapis/protodeps/${library}.deps" || true
  done
  for file in "${files[@]}"; do
    LC_ALL=C sort -u "${file}" >"${file}.sorted"
    mv "${file}.sorted" "${file}"
  done
done

# TODO(#11694) - the Bazel file introduces a dependency that we do not build.
#    Fortunately (maybe?) we do not use or need the dep until the issue is
#    resolved.
sed -i '/@googleapis\/\/google\/cloud\/location:location_proto/d' \
  "external/googleapis/protodeps/datamigration.deps"
