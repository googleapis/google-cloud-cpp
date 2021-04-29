# ~~~
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
# ~~~

cmake_minimum_required(VERSION 3.5)

# Give application developers a hook to configure the version and hash
# downloaded from GitHub.
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL
    "https://github.com/googleapis/googleapis/archive/6ce40ff8faf68226782f507ca6b2d497a77044de.tar.gz"
)
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    "6762083f829f998c3971efa2ba858c21d4ac4ba77feb9650bad7d358e3add2a5")

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${CMAKE_BINARY_DIR}/external/googleapis/src/googleapis_download")

set(EXTERNAL_GOOGLEAPIS_PROTO_FILES
    "google/api/http.proto"
    "google/api/annotations.proto"
    "google/api/auth.proto"
    "google/api/client.proto"
    "google/api/distribution.proto"
    "google/api/field_behavior.proto"
    "google/api/label.proto"
    "google/api/launch_stage.proto"
    "google/api/metric.proto"
    "google/api/monitored_resource.proto"
    "google/api/resource.proto"
    "google/devtools/cloudtrace/v2/trace.proto"
    "google/devtools/cloudtrace/v2/tracing.proto"
    "google/rpc/error_details.proto"
    "google/rpc/status.proto"
    "google/iam/credentials/v1/common.proto"
    "google/iam/credentials/v1/iamcredentials.proto"
    "google/iam/v1/options.proto"
    "google/iam/v1/policy.proto"
    "google/iam/v1/iam_policy.proto"
    "google/longrunning/operations.proto"
    "google/bigtable/admin/v2/bigtable_instance_admin.proto"
    "google/bigtable/admin/v2/bigtable_table_admin.proto"
    "google/bigtable/admin/v2/common.proto"
    "google/bigtable/admin/v2/instance.proto"
    "google/bigtable/admin/v2/table.proto"
    "google/bigtable/v2/bigtable.proto"
    "google/bigtable/v2/data.proto"
    "google/cloud/bigquery/connection/v1beta1/connection.proto"
    "google/cloud/bigquery/datatransfer/v1/datatransfer.proto"
    "google/cloud/bigquery/datatransfer/v1/transfer.proto"
    "google/cloud/bigquery/logging/v1/audit_data.proto"
    "google/cloud/bigquery/storage/v1/arrow.proto"
    "google/cloud/bigquery/storage/v1/avro.proto"
    "google/cloud/bigquery/storage/v1/storage.proto"
    "google/cloud/bigquery/storage/v1/stream.proto"
    "google/cloud/bigquery/v2/encryption_config.proto"
    "google/cloud/bigquery/v2/model.proto"
    "google/cloud/bigquery/v2/model_reference.proto"
    "google/cloud/bigquery/v2/standard_sql.proto"
    "google/cloud/bigquery/v2/table_reference.proto"
    "google/cloud/dialogflow/v2beta1/agent.proto"
    "google/cloud/dialogflow/v2beta1/audio_config.proto"
    "google/cloud/dialogflow/v2beta1/context.proto"
    "google/cloud/dialogflow/v2beta1/document.proto"
    "google/cloud/dialogflow/v2beta1/entity_type.proto"
    "google/cloud/dialogflow/v2beta1/environment.proto"
    "google/cloud/dialogflow/v2beta1/gcs.proto"
    "google/cloud/dialogflow/v2beta1/intent.proto"
    "google/cloud/dialogflow/v2beta1/knowledge_base.proto"
    "google/cloud/dialogflow/v2beta1/session.proto"
    "google/cloud/dialogflow/v2beta1/session_entity_type.proto"
    "google/cloud/dialogflow/v2beta1/validation_result.proto"
    "google/cloud/dialogflow/v2beta1/webhook.proto"
    "google/cloud/dialogflow/v2/agent.proto"
    "google/cloud/dialogflow/v2/audio_config.proto"
    "google/cloud/dialogflow/v2/context.proto"
    "google/cloud/dialogflow/v2/entity_type.proto"
    "google/cloud/dialogflow/v2/environment.proto"
    "google/cloud/dialogflow/v2/intent.proto"
    "google/cloud/dialogflow/v2/session.proto"
    "google/cloud/dialogflow/v2/session_entity_type.proto"
    "google/cloud/dialogflow/v2/validation_result.proto"
    "google/cloud/dialogflow/v2/webhook.proto"
    "google/cloud/speech/v1/cloud_speech.proto"
    "google/cloud/texttospeech/v1/cloud_tts.proto"
    "google/logging/type/http_request.proto"
    "google/logging/type/log_severity.proto"
    "google/logging/v2/log_entry.proto"
    "google/logging/v2/logging.proto"
    "google/logging/v2/logging_config.proto"
    "google/logging/v2/logging_metrics.proto"
    "google/monitoring/v3/alert.proto"
    "google/monitoring/v3/alert_service.proto"
    "google/monitoring/v3/common.proto"
    "google/monitoring/v3/dropped_labels.proto"
    "google/monitoring/v3/group.proto"
    "google/monitoring/v3/group_service.proto"
    "google/monitoring/v3/metric.proto"
    "google/monitoring/v3/metric_service.proto"
    "google/monitoring/v3/mutation_record.proto"
    "google/monitoring/v3/notification.proto"
    "google/monitoring/v3/notification_service.proto"
    "google/monitoring/v3/service.proto"
    "google/monitoring/v3/service_service.proto"
    "google/monitoring/v3/span_context.proto"
    "google/monitoring/v3/uptime.proto"
    "google/monitoring/v3/uptime_service.proto"
    "google/pubsub/v1/pubsub.proto"
    "google/pubsub/v1/schema.proto"
    "google/spanner/admin/database/v1/backup.proto"
    "google/spanner/admin/database/v1/common.proto"
    "google/spanner/admin/database/v1/spanner_database_admin.proto"
    "google/spanner/admin/instance/v1/spanner_instance_admin.proto"
    "google/spanner/v1/keys.proto"
    "google/spanner/v1/mutation.proto"
    "google/spanner/v1/query_plan.proto"
    "google/spanner/v1/result_set.proto"
    "google/spanner/v1/spanner.proto"
    "google/spanner/v1/transaction.proto"
    "google/spanner/v1/type.proto"
    "google/storage/v1/storage.proto"
    "google/storage/v1/storage_resources.proto"
    "google/type/calendar_period.proto"
    "google/type/color.proto"
    "google/type/date.proto"
    "google/type/datetime.proto"
    "google/type/dayofweek.proto"
    "google/type/expr.proto"
    "google/type/fraction.proto"
    "google/type/interval.proto"
    "google/type/latlng.proto"
    "google/type/localized_text.proto"
    "google/type/money.proto"
    "google/type/month.proto"
    "google/type/phone_number.proto"
    "google/type/postal_address.proto"
    "google/type/quaternion.proto"
    "google/type/timeofday.proto")

include(EnableWerror)

set(EXTERNAL_GOOGLEAPIS_BYPRODUCTS)
foreach (proto ${EXTERNAL_GOOGLEAPIS_PROTO_FILES})
    list(APPEND EXTERNAL_GOOGLEAPIS_BYPRODUCTS
         "${EXTERNAL_GOOGLEAPIS_SOURCE}/${proto}")
endforeach ()

include(ExternalProject)

ExternalProject_Add(
    googleapis_download
    EXCLUDE_FROM_ALL ON
    PREFIX "${CMAKE_BINARY_DIR}/external/googleapis"
    URL ${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL}
    URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256}
    PATCH_COMMAND
        ""
        # ~~~
        # Scaffolding for patching googleapis after download. For example:
        #   PATCH_COMMAND
        #       patch
        #       -p1
        #       --input=/workspace/external/googleapis.patch
        # NOTE: This should only be used while developing with a new
        # protobuf message. No changes to `PATCH_COMMAND` should ever be
        # committed to the main branch.
        # ~~~
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${EXTERNAL_GOOGLEAPIS_BYPRODUCTS}
    LOG_DOWNLOAD OFF)

find_package(ProtobufWithTargets REQUIRED)
find_package(gRPC REQUIRED)

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(SelectMSVCRuntime)

# Include the functions to compile proto files.
include(CompileProtos)

google_cloud_cpp_add_protos_property()

function (external_googleapis_short_name var proto)
    string(REPLACE "google/" "" short_name "${proto}")
    string(REPLACE "/" "_" short_name "${short_name}")
    string(REPLACE ".proto" "_protos" short_name "${short_name}")
    set("${var}"
        "${short_name}"
        PARENT_SCOPE)
endfunction ()

# Create a single source proto library.
#
# * proto: the filename for the proto source.
# * (optional) ARGN: proto libraries the new library depends on.
function (external_googleapis_add_library proto)
    external_googleapis_short_name(short_name "${proto}")
    google_cloud_cpp_grpcpp_library(
        google_cloud_cpp_${short_name} "${EXTERNAL_GOOGLEAPIS_SOURCE}/${proto}"
        PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
        "${PROTO_INCLUDE_DIR}")

    external_googleapis_set_version_and_alias("${short_name}")

    set(public_deps)
    foreach (dep_short_name ${ARGN})
        list(APPEND public_deps "google-cloud-cpp::${dep_short_name}")
    endforeach ()
    list(LENGTH public_deps public_deps_length)
    if (public_deps_length EQUAL 0)
        target_link_libraries("google_cloud_cpp_${short_name}")
    else ()
        target_link_libraries("google_cloud_cpp_${short_name}"
                              PUBLIC ${public_deps})
    endif ()
endfunction ()

function (external_googleapis_set_version_and_alias short_name)
    add_dependencies("google_cloud_cpp_${short_name}" googleapis_download)
    set_target_properties(
        "google_cloud_cpp_${short_name}"
        PROPERTIES EXPORT_NAME google-cloud-cpp::${short_name}
                   VERSION "${PROJECT_VERSION}" SOVERSION
                                                ${PROJECT_VERSION_MAJOR})
    add_library("google-cloud-cpp::${short_name}" ALIAS
                "google_cloud_cpp_${short_name}")
endfunction ()

set(external_googleapis_installed_libraries_list
    google_cloud_cpp_bigtable_protos
    google_cloud_cpp_cloud_bigquery_protos
    google_cloud_cpp_cloud_speech_protos
    google_cloud_cpp_cloud_texttospeech_protos
    google_cloud_cpp_iam_protos
    google_cloud_cpp_pubsub_protos
    google_cloud_cpp_spanner_protos
    google_cloud_cpp_storage_protos
    google_cloud_cpp_longrunning_operations_protos
    google_cloud_cpp_api_http_protos
    google_cloud_cpp_api_annotations_protos
    google_cloud_cpp_api_auth_protos
    google_cloud_cpp_api_client_protos
    google_cloud_cpp_api_distribution_protos
    google_cloud_cpp_api_field_behavior_protos
    google_cloud_cpp_api_label_protos
    google_cloud_cpp_api_launch_stage_protos
    google_cloud_cpp_api_metric_protos
    google_cloud_cpp_api_monitored_resource_protos
    google_cloud_cpp_api_resource_protos
    google_cloud_cpp_devtools_cloudtrace_v2_trace_protos
    google_cloud_cpp_devtools_cloudtrace_v2_tracing_protos
    google_cloud_cpp_logging_type_protos
    google_cloud_cpp_logging_protos
    google_cloud_cpp_monitoring_protos
    google_cloud_cpp_iam_v1_options_protos
    google_cloud_cpp_iam_v1_policy_protos
    google_cloud_cpp_iam_v1_iam_policy_protos
    google_cloud_cpp_rpc_error_details_protos
    google_cloud_cpp_rpc_status_protos
    google_cloud_cpp_type_calendar_period_protos
    google_cloud_cpp_type_color_protos
    google_cloud_cpp_type_date_protos
    google_cloud_cpp_type_datetime_protos
    google_cloud_cpp_type_dayofweek_protos
    google_cloud_cpp_type_expr_protos
    google_cloud_cpp_type_fraction_protos
    google_cloud_cpp_type_interval_protos
    google_cloud_cpp_type_latlng_protos
    google_cloud_cpp_type_localized_text_protos
    google_cloud_cpp_type_money_protos
    google_cloud_cpp_type_month_protos
    google_cloud_cpp_type_phone_number_protos
    google_cloud_cpp_type_postal_address_protos
    google_cloud_cpp_type_quaternion_protos
    google_cloud_cpp_type_timeofday_protos)

external_googleapis_add_library("google/api/http.proto")
external_googleapis_add_library("google/api/metric.proto"
                                api_launch_stage_protos api_label_protos)
external_googleapis_add_library("google/api/monitored_resource.proto"
                                api_launch_stage_protos api_label_protos)
external_googleapis_add_library("google/api/annotations.proto" api_http_protos)
external_googleapis_add_library("google/api/auth.proto" api_annotations_protos)
external_googleapis_add_library("google/api/client.proto")
external_googleapis_add_library("google/api/distribution.proto")
external_googleapis_add_library("google/api/field_behavior.proto")
external_googleapis_add_library("google/api/label.proto")
external_googleapis_add_library("google/api/launch_stage.proto")
external_googleapis_add_library("google/api/resource.proto")

external_googleapis_add_library("google/type/calendar_period.proto")
external_googleapis_add_library("google/type/color.proto")
external_googleapis_add_library("google/type/date.proto")
external_googleapis_add_library("google/type/datetime.proto")
external_googleapis_add_library("google/type/dayofweek.proto")
external_googleapis_add_library("google/type/expr.proto")
external_googleapis_add_library("google/type/fraction.proto")
external_googleapis_add_library("google/type/interval.proto")
external_googleapis_add_library("google/type/latlng.proto")
external_googleapis_add_library("google/type/localized_text.proto")
external_googleapis_add_library("google/type/money.proto")
external_googleapis_add_library("google/type/month.proto")
external_googleapis_add_library("google/type/phone_number.proto")
external_googleapis_add_library("google/type/postal_address.proto")
external_googleapis_add_library("google/type/quaternion.proto")
external_googleapis_add_library("google/type/timeofday.proto")

external_googleapis_add_library("google/rpc/error_details.proto")
external_googleapis_add_library("google/rpc/status.proto"
                                rpc_error_details_protos)

external_googleapis_add_library("google/iam/v1/options.proto"
                                api_annotations_protos)
external_googleapis_add_library("google/iam/v1/policy.proto"
                                api_annotations_protos type_expr_protos)

external_googleapis_add_library(
    "google/iam/v1/iam_policy.proto"
    api_annotations_protos
    api_client_protos
    api_field_behavior_protos
    api_resource_protos
    iam_v1_options_protos
    iam_v1_policy_protos)

external_googleapis_add_library(
    "google/longrunning/operations.proto" api_annotations_protos
    api_client_protos rpc_status_protos)

external_googleapis_add_library(
    "google/devtools/cloudtrace/v2/trace.proto" api_annotations_protos
    api_field_behavior_protos api_resource_protos rpc_status_protos)
external_googleapis_add_library(
    "google/devtools/cloudtrace/v2/tracing.proto"
    devtools_cloudtrace_v2_trace_protos api_annotations_protos
    api_client_protos api_field_behavior_protos rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_bigquery_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/connection/v1beta1/connection.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/datatransfer/v1/datatransfer.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/datatransfer/v1/transfer.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/logging/v1/audit_data.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/storage/v1/arrow.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/storage/v1/avro.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/storage/v1/storage.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/storage/v1/stream.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/v2/encryption_config.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/v2/model.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/v2/model_reference.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/v2/standard_sql.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/bigquery/v2/table_reference.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_bigquery_protos)
target_link_libraries(
    google_cloud_cpp_cloud_bigquery_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::rpc_status_protos
           google-cloud-cpp::api_http_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_bigtable_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/admin/v2/bigtable_instance_admin.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/admin/v2/bigtable_table_admin.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/admin/v2/common.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/admin/v2/instance.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/admin/v2/table.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/v2/bigtable.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/v2/data.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(bigtable_protos)
target_link_libraries(
    google_cloud_cpp_bigtable_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos
           google-cloud-cpp::api_auth_protos)

# TODO(#5660) - protobuf + gcc<6 + deprecated enums do not compile
if (NOT (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
         AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)))

    google_cloud_cpp_grpcpp_library(
        google_cloud_cpp_cloud_dialogflow_v2_protos
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/agent.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/audio_config.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/context.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/entity_type.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/environment.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/intent.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/session.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/session_entity_type.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/validation_result.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2/webhook.proto"
        PROTO_PATH_DIRECTORIES
        "${EXTERNAL_GOOGLEAPIS_SOURCE}"
        "${PROTO_INCLUDE_DIR}")
    external_googleapis_set_version_and_alias(cloud_dialogflow_v2_protos)
    target_link_libraries(
        google_cloud_cpp_cloud_dialogflow_v2_protos
        PUBLIC google-cloud-cpp::api_annotations_protos
               google-cloud-cpp::api_client_protos
               google-cloud-cpp::api_field_behavior_protos
               google-cloud-cpp::api_resource_protos
               google-cloud-cpp::longrunning_operations_protos
               google-cloud-cpp::rpc_status_protos
               google-cloud-cpp::type_latlng_protos)

    google_cloud_cpp_grpcpp_library(
        google_cloud_cpp_cloud_dialogflow_v2beta1_protos
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/agent.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/audio_config.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/context.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/document.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/entity_type.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/environment.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/gcs.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/intent.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/knowledge_base.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/session.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/session_entity_type.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/validation_result.proto"
        "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/dialogflow/v2beta1/webhook.proto"
        PROTO_PATH_DIRECTORIES
        "${EXTERNAL_GOOGLEAPIS_SOURCE}"
        "${PROTO_INCLUDE_DIR}")
    external_googleapis_set_version_and_alias(cloud_dialogflow_v2beta1_protos)
    target_link_libraries(
        google_cloud_cpp_cloud_dialogflow_v2beta1_protos
        PUBLIC google-cloud-cpp::api_annotations_protos
               google-cloud-cpp::api_client_protos
               google-cloud-cpp::api_field_behavior_protos
               google-cloud-cpp::api_resource_protos
               google-cloud-cpp::longrunning_operations_protos
               google-cloud-cpp::rpc_status_protos
               google-cloud-cpp::type_latlng_protos)

    list(APPEND external_googleapis_installed_libraries_list
         google_cloud_cpp_cloud_dialogflow_v2_protos
         google_cloud_cpp_cloud_dialogflow_v2beta1_protos)
endif ()

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_speech_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/speech/v1/cloud_speech.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_speech_protos)
target_link_libraries(
    google_cloud_cpp_cloud_speech_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_texttospeech_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/texttospeech/v1/cloud_tts.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_texttospeech_protos)
target_link_libraries(
    google_cloud_cpp_cloud_texttospeech_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_iam_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/iam/credentials/v1/iamcredentials.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/iam/credentials/v1/common.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(iam_protos)
target_link_libraries(
    google_cloud_cpp_iam_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_logging_type_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/type/http_request.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/type/log_severity.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(logging_type_protos)
target_link_libraries(google_cloud_cpp_logging_type_protos
                      PUBLIC google-cloud-cpp::api_annotations_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_logging_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/v2/log_entry.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/v2/logging.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/v2/logging_config.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/logging/v2/logging_metrics.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(logging_protos)
target_link_libraries(
    google_cloud_cpp_logging_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_distribution_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_metric_protos
           google-cloud-cpp::api_monitored_resource_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::logging_type_protos
           google-cloud-cpp::rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_monitoring_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/alert.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/alert_service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/common.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/dropped_labels.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/group.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/group_service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/metric.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/metric_service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/mutation_record.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/notification.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/notification_service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/service_service.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/span_context.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/uptime.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/monitoring/v3/uptime_service.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(monitoring_protos)
target_link_libraries(
    google_cloud_cpp_monitoring_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_distribution_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_label_protos
           google-cloud-cpp::api_launch_stage_protos
           google-cloud-cpp::api_metric_protos
           google-cloud-cpp::api_monitored_resource_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::rpc_status_protos
           google-cloud-cpp::type_calendar_period_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_pubsub_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/pubsub/v1/pubsub.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/pubsub/v1/schema.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(pubsub_protos)
target_link_libraries(
    google_cloud_cpp_pubsub_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_spanner_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/admin/database/v1/backup.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/admin/database/v1/common.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/admin/database/v1/spanner_database_admin.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/admin/instance/v1/spanner_instance_admin.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/keys.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/mutation.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/query_plan.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/result_set.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/spanner.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/transaction.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/spanner/v1/type.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(spanner_protos)
target_link_libraries(
    google_cloud_cpp_spanner_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_storage_protos
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/storage/v1/storage.proto"
    "${EXTERNAL_GOOGLEAPIS_SOURCE}/google/storage/v1/storage_resources.proto"
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(storage_protos)
target_link_libraries(
    google_cloud_cpp_storage_protos
    PUBLIC google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::iam_v1_iam_policy_protos
           google-cloud-cpp::iam_v1_policy_protos)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
include(GNUInstallDirs)

install(
    TARGETS ${external_googleapis_installed_libraries_list}
    EXPORT googleapis-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS ${external_googleapis_installed_libraries_list}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

foreach (target ${external_googleapis_installed_libraries_list})
    google_cloud_cpp_install_proto_library_headers("${target}")
    google_cloud_cpp_install_proto_library_protos(
        "${target}" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
endforeach ()

# Use a function to create a scope for the variables.
function (external_googleapis_install_pc target)
    string(REPLACE "google_cloud_cpp_" "" _short_name ${target})
    string(REPLACE "_protos" "" _short_name ${_short_name})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "The Google APIS C++ ${_short_name} Proto Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Compiled proto for C++.")
    # Examine the target LINK_LIBRARIES property, use that to pull the
    # dependencies between the google-cloud-cpp::* libraries.
    set(_target_pc_requires)
    get_target_property(_target_deps ${target} LINK_LIBRARIES)
    foreach (dep ${_target_deps})
        if ("${dep}" MATCHES "^google-cloud-cpp::")
            string(REPLACE "google-cloud-cpp::" "google_cloud_cpp_" dep
                           "${dep}")
            list(APPEND _target_pc_requires " " "${dep}")
        endif ()
    endforeach ()
    # These dependencies are required for all the google-cloud-cpp::* libraries.
    list(
        APPEND
        _target_pc_requires
        " grpc++"
        " grpc"
        " openssl"
        " protobuf"
        " zlib"
        " libcares")
    string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES ${_target_pc_requires})
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-l${target}")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in" "${target}.pc"
                   @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT google_cloud_cpp_development)
    # TODO(#5726) - also install the module with the legacy name
    string(REPLACE "google_cloud_cpp_" "googleapis_cpp_" legacy ${target})
    configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in" "${legacy}.pc"
                   @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${legacy}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT google_cloud_cpp_development)
endfunction ()

# Create and install the pkg-config files.
foreach (target ${external_googleapis_installed_libraries_list})
    external_googleapis_install_pc("${target}")
endforeach ()

# Create and install the googleapis pkg-config file for backwards compatibility.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google APIS C++ Proto Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Platforms.")
# Note the use of spaces, `string(JOIN)` is not available in cmake-3.5, so we
# need to add the separator ourselves.
string(
    CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES
           "google_cloud_cpp_bigtable_protos"
           " google_cloud_cpp_cloud_bigquery_protos"
           " google_cloud_cpp_iam_protos"
           " google_cloud_cpp_pubsub_protos"
           " google_cloud_cpp_storage_protos"
           " google_cloud_cpp_logging_protos"
           " google_cloud_cpp_iam_v1_iam_policy_protos"
           " google_cloud_cpp_iam_v1_options_protos"
           " google_cloud_cpp_iam_v1_policy_protos"
           " google_cloud_cpp_longrunning_operations_protos"
           " google_cloud_cpp_api_auth_protos"
           " google_cloud_cpp_api_annotations_protos"
           " google_cloud_cpp_api_client_protos"
           " google_cloud_cpp_api_field_behavior_protos"
           " google_cloud_cpp_api_http_protos"
           " google_cloud_cpp_rpc_status_protos"
           " google_cloud_cpp_rpc_error_details_protos"
           " grpc++"
           " grpc"
           " openssl"
           " protobuf"
           " zlib"
           " libcares")
set(GOOGLE_CLOUD_CPP_PC_LIBS "")
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in" "googleapis.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)

configure_file("${CMAKE_CURRENT_LIST_DIR}/config.cmake.in"
               "google_cloud_cpp_googleapis-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_googleapis-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT googleapis-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_googleapis"
    COMPONENT google_cloud_cpp_development)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_googleapis-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_googleapis-config-version.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/FindgRPC.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/FindProtobufWithTargets.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_googleapis"
    COMPONENT google_cloud_cpp_development)

# TODO(#5726) - we need to install the legacy CMake configuration under the
# googleapis-c++ names:
configure_file("${CMAKE_CURRENT_LIST_DIR}/legacy.cmake.in"
               "googleapis-config.cmake" @ONLY)
write_basic_package_version_file(
    "googleapis-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis"
    COMPONENT google_cloud_cpp_development)
