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
    "https://github.com/googleapis/googleapis/archive/c8bfd324b41ad1f6f65fed124572f92fe116517b.tar.gz"
)
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
    "712b622bb93aa07051a81ac669f723c17d988568521f69104ee1627e5a4a2823")

set(GOOGLEAPIS_CPP_SOURCE
    "${CMAKE_BINARY_DIR}/external/googleapis/src/googleapis_download")

set(GOOGLEAPIS_CPP_PROTO_FILES
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
    "google/cloud/bigquery/storage/v1beta1/arrow.proto"
    "google/cloud/bigquery/storage/v1beta1/avro.proto"
    "google/cloud/bigquery/storage/v1beta1/read_options.proto"
    "google/cloud/bigquery/storage/v1beta1/storage.proto"
    "google/cloud/bigquery/storage/v1beta1/table_reference.proto"
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

# Always disable clang-tidy for this generated code.
set(CMAKE_CXX_CLANG_TIDY "")

set(GOOGLEAPIS_CPP_BYPRODUCTS)
foreach (proto ${GOOGLEAPIS_CPP_PROTO_FILES})
    list(APPEND GOOGLEAPIS_CPP_BYPRODUCTS "${GOOGLEAPIS_CPP_SOURCE}/${proto}")
endforeach ()

include(ExternalProject)
ExternalProject_Add(
    googleapis_download
    EXCLUDE_FROM_ALL ON
    PREFIX "${CMAKE_BINARY_DIR}/external/googleapis"
    URL ${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL}
    URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${GOOGLEAPIS_CPP_BYPRODUCTS}
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

add_library(googleapis_cpp_common_flags INTERFACE)

include(SelectMSVCRuntime)

# Include the functions to compile proto files.
include(CompileProtos)

google_cloud_cpp_add_protos_property()

function (googleapis_cpp_short_name var proto)
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
function (googleapis_cpp_add_library proto)
    googleapis_cpp_short_name(short_name "${proto}")
    google_cloud_cpp_grpcpp_library(
        googleapis_cpp_${short_name} "${GOOGLEAPIS_CPP_SOURCE}/${proto}"
        PROTO_PATH_DIRECTORIES "${GOOGLEAPIS_CPP_SOURCE}"
        "${PROTO_INCLUDE_DIR}")

    googleapis_cpp_set_version_and_alias("${short_name}")

    set(public_deps)
    foreach (dep_short_name ${ARGN})
        list(APPEND public_deps "googleapis-c++::${dep_short_name}")
    endforeach ()
    list(LENGTH public_deps public_deps_length)
    if (public_deps_length EQUAL 0)
        target_link_libraries("googleapis_cpp_${short_name}"
                              PRIVATE googleapis_cpp_common_flags)
    else ()
        target_link_libraries(
            "googleapis_cpp_${short_name}"
            PUBLIC ${public_deps}
            PRIVATE googleapis_cpp_common_flags)
    endif ()
endfunction ()

function (googleapis_cpp_set_version_and_alias short_name)
    add_dependencies("googleapis_cpp_${short_name}" googleapis_download)
    set_target_properties(
        "googleapis_cpp_${short_name}"
        PROPERTIES VERSION "${PROJECT_VERSION}" SOVERSION
                                                ${PROJECT_VERSION_MAJOR})
    set_target_properties("googleapis_cpp_${short_name}"
                          PROPERTIES EXPORT_NAME googleapis-c++::${short_name})
    add_library("googleapis-c++::${short_name}" ALIAS
                "googleapis_cpp_${short_name}")
endfunction ()

set(googleapis_cpp_installed_libraries_list
    googleapis_cpp_bigtable_protos
    googleapis_cpp_cloud_bigquery_protos
    googleapis_cpp_cloud_speech_protos
    googleapis_cpp_cloud_texttospeech_protos
    googleapis_cpp_iam_protos
    googleapis_cpp_pubsub_protos
    googleapis_cpp_spanner_protos
    googleapis_cpp_storage_protos
    googleapis_cpp_longrunning_operations_protos
    googleapis_cpp_api_http_protos
    googleapis_cpp_api_annotations_protos
    googleapis_cpp_api_auth_protos
    googleapis_cpp_api_client_protos
    googleapis_cpp_api_distribution_protos
    googleapis_cpp_api_field_behavior_protos
    googleapis_cpp_api_label_protos
    googleapis_cpp_api_launch_stage_protos
    googleapis_cpp_api_metric_protos
    googleapis_cpp_api_monitored_resource_protos
    googleapis_cpp_api_resource_protos
    googleapis_cpp_devtools_cloudtrace_v2_trace_protos
    googleapis_cpp_devtools_cloudtrace_v2_tracing_protos
    googleapis_cpp_logging_type_protos
    googleapis_cpp_logging_protos
    googleapis_cpp_monitoring_protos
    googleapis_cpp_iam_v1_options_protos
    googleapis_cpp_iam_v1_policy_protos
    googleapis_cpp_iam_v1_iam_policy_protos
    googleapis_cpp_rpc_error_details_protos
    googleapis_cpp_rpc_status_protos
    googleapis_cpp_type_calendar_period_protos
    googleapis_cpp_type_color_protos
    googleapis_cpp_type_date_protos
    googleapis_cpp_type_datetime_protos
    googleapis_cpp_type_dayofweek_protos
    googleapis_cpp_type_expr_protos
    googleapis_cpp_type_fraction_protos
    googleapis_cpp_type_interval_protos
    googleapis_cpp_type_latlng_protos
    googleapis_cpp_type_localized_text_protos
    googleapis_cpp_type_money_protos
    googleapis_cpp_type_month_protos
    googleapis_cpp_type_phone_number_protos
    googleapis_cpp_type_postal_address_protos
    googleapis_cpp_type_quaternion_protos
    googleapis_cpp_type_timeofday_protos)

googleapis_cpp_add_library("google/api/http.proto")
googleapis_cpp_add_library("google/api/metric.proto" api_launch_stage_protos
                           api_label_protos)
googleapis_cpp_add_library("google/api/monitored_resource.proto"
                           api_launch_stage_protos api_label_protos)
googleapis_cpp_add_library("google/api/annotations.proto" api_http_protos)
googleapis_cpp_add_library("google/api/auth.proto" api_annotations_protos)
googleapis_cpp_add_library("google/api/client.proto")
googleapis_cpp_add_library("google/api/distribution.proto")
googleapis_cpp_add_library("google/api/field_behavior.proto")
googleapis_cpp_add_library("google/api/label.proto")
googleapis_cpp_add_library("google/api/launch_stage.proto")
googleapis_cpp_add_library("google/api/resource.proto")

googleapis_cpp_add_library("google/type/calendar_period.proto")
googleapis_cpp_add_library("google/type/color.proto")
googleapis_cpp_add_library("google/type/date.proto")
googleapis_cpp_add_library("google/type/datetime.proto")
googleapis_cpp_add_library("google/type/dayofweek.proto")
googleapis_cpp_add_library("google/type/expr.proto")
googleapis_cpp_add_library("google/type/fraction.proto")
googleapis_cpp_add_library("google/type/interval.proto")
googleapis_cpp_add_library("google/type/latlng.proto")
googleapis_cpp_add_library("google/type/localized_text.proto")
googleapis_cpp_add_library("google/type/money.proto")
googleapis_cpp_add_library("google/type/month.proto")
googleapis_cpp_add_library("google/type/phone_number.proto")
googleapis_cpp_add_library("google/type/postal_address.proto")
googleapis_cpp_add_library("google/type/quaternion.proto")
googleapis_cpp_add_library("google/type/timeofday.proto")

googleapis_cpp_add_library("google/rpc/error_details.proto")
googleapis_cpp_add_library("google/rpc/status.proto" rpc_error_details_protos)

googleapis_cpp_add_library("google/iam/v1/options.proto" api_annotations_protos)
googleapis_cpp_add_library("google/iam/v1/policy.proto" api_annotations_protos
                           type_expr_protos)

googleapis_cpp_add_library(
    "google/iam/v1/iam_policy.proto"
    api_annotations_protos
    api_client_protos
    api_field_behavior_protos
    api_resource_protos
    iam_v1_options_protos
    iam_v1_policy_protos)

googleapis_cpp_add_library(
    "google/longrunning/operations.proto" api_annotations_protos
    api_client_protos rpc_status_protos)

googleapis_cpp_add_library(
    "google/devtools/cloudtrace/v2/trace.proto" api_annotations_protos
    api_field_behavior_protos api_resource_protos rpc_status_protos)
googleapis_cpp_add_library(
    "google/devtools/cloudtrace/v2/tracing.proto"
    devtools_cloudtrace_v2_trace_protos api_annotations_protos
    api_client_protos api_field_behavior_protos rpc_status_protos)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_cloud_bigquery_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/connection/v1beta1/connection.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/datatransfer/v1/datatransfer.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/datatransfer/v1/transfer.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/logging/v1/audit_data.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/storage/v1beta1/arrow.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/storage/v1beta1/avro.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/storage/v1beta1/read_options.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/storage/v1beta1/storage.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/storage/v1beta1/table_reference.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/v2/encryption_config.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/v2/model.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/v2/model_reference.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/v2/standard_sql.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/bigquery/v2/table_reference.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(cloud_bigquery_protos)
target_link_libraries(
    googleapis_cpp_cloud_bigquery_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_resource_protos
           googleapis-c++::iam_v1_iam_policy_protos
           googleapis-c++::iam_v1_policy_protos
           googleapis-c++::rpc_status_protos
           googleapis-c++::api_http_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_bigtable_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/admin/v2/bigtable_instance_admin.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/admin/v2/bigtable_table_admin.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/admin/v2/common.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/admin/v2/instance.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/admin/v2/table.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/v2/bigtable.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/bigtable/v2/data.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(bigtable_protos)
target_link_libraries(
    googleapis_cpp_bigtable_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_resource_protos
           googleapis-c++::iam_v1_iam_policy_protos
           googleapis-c++::iam_v1_policy_protos
           googleapis-c++::longrunning_operations_protos
           googleapis-c++::rpc_status_protos
           googleapis-c++::api_auth_protos
    PRIVATE googleapis_cpp_common_flags)

# TODO(#5660) - protobuf + gcc<6 + deprecated enums do not compile
if (NOT (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
         AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)))

    google_cloud_cpp_grpcpp_library(
        googleapis_cpp_cloud_dialogflow_v2_protos
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/agent.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/audio_config.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/context.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/entity_type.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/environment.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/intent.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/session.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/session_entity_type.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/validation_result.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2/webhook.proto"
        PROTO_PATH_DIRECTORIES
        "${GOOGLEAPIS_CPP_SOURCE}"
        "${PROTO_INCLUDE_DIR}")
    googleapis_cpp_set_version_and_alias(cloud_dialogflow_v2_protos)
    target_link_libraries(
        googleapis_cpp_cloud_dialogflow_v2_protos
        PUBLIC googleapis-c++::api_annotations_protos
               googleapis-c++::api_client_protos
               googleapis-c++::api_field_behavior_protos
               googleapis-c++::api_resource_protos
               googleapis-c++::longrunning_operations_protos
               googleapis-c++::rpc_status_protos
               googleapis-c++::type_latlng_protos
        PRIVATE googleapis_cpp_common_flags)

    google_cloud_cpp_grpcpp_library(
        googleapis_cpp_cloud_dialogflow_v2beta1_protos
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/agent.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/audio_config.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/context.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/document.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/entity_type.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/environment.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/gcs.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/intent.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/knowledge_base.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/session.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/session_entity_type.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/validation_result.proto"
        "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/dialogflow/v2beta1/webhook.proto"
        PROTO_PATH_DIRECTORIES
        "${GOOGLEAPIS_CPP_SOURCE}"
        "${PROTO_INCLUDE_DIR}")
    googleapis_cpp_set_version_and_alias(cloud_dialogflow_v2beta1_protos)
    target_link_libraries(
        googleapis_cpp_cloud_dialogflow_v2beta1_protos
        PUBLIC googleapis-c++::api_annotations_protos
               googleapis-c++::api_client_protos
               googleapis-c++::api_field_behavior_protos
               googleapis-c++::api_resource_protos
               googleapis-c++::longrunning_operations_protos
               googleapis-c++::rpc_status_protos
               googleapis-c++::type_latlng_protos
        PRIVATE googleapis_cpp_common_flags)

    list(APPEND googleapis_cpp_installed_libraries_list
         googleapis_cpp_cloud_dialogflow_v2_protos
         googleapis_cpp_cloud_dialogflow_v2beta1_protos)
endif ()

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_cloud_speech_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/speech/v1/cloud_speech.proto"
    PROTO_PATH_DIRECTORIES "${GOOGLEAPIS_CPP_SOURCE}" "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(cloud_speech_protos)
target_link_libraries(
    googleapis_cpp_cloud_speech_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::longrunning_operations_protos
           googleapis-c++::rpc_status_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_cloud_texttospeech_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/cloud/texttospeech/v1/cloud_tts.proto"
    PROTO_PATH_DIRECTORIES "${GOOGLEAPIS_CPP_SOURCE}" "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(cloud_texttospeech_protos)
target_link_libraries(
    googleapis_cpp_cloud_texttospeech_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_iam_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/iam/credentials/v1/iamcredentials.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/iam/credentials/v1/common.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(iam_protos)
target_link_libraries(
    googleapis_cpp_iam_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_resource_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_logging_type_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/type/http_request.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/type/log_severity.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(logging_type_protos)
target_link_libraries(
    googleapis_cpp_logging_type_protos
    PUBLIC googleapis-c++::api_annotations_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_logging_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/v2/log_entry.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/v2/logging.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/v2/logging_config.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/logging/v2/logging_metrics.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(logging_protos)
target_link_libraries(
    googleapis_cpp_logging_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_distribution_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_metric_protos
           googleapis-c++::api_monitored_resource_protos
           googleapis-c++::api_resource_protos
           googleapis-c++::logging_type_protos
           googleapis-c++::rpc_status_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_monitoring_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/alert.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/alert_service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/common.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/dropped_labels.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/group.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/group_service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/metric.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/metric_service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/mutation_record.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/notification.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/notification_service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/service_service.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/span_context.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/uptime.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/monitoring/v3/uptime_service.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(monitoring_protos)
target_link_libraries(
    googleapis_cpp_monitoring_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_distribution_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_label_protos
           googleapis-c++::api_launch_stage_protos
           googleapis-c++::api_metric_protos
           googleapis-c++::api_monitored_resource_protos
           googleapis-c++::api_resource_protos
           googleapis-c++::rpc_status_protos
           googleapis-c++::type_calendar_period_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_pubsub_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/pubsub/v1/pubsub.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/pubsub/v1/schema.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(pubsub_protos)
target_link_libraries(
    googleapis_cpp_pubsub_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_resource_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_spanner_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/admin/database/v1/backup.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/admin/database/v1/common.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/admin/database/v1/spanner_database_admin.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/admin/instance/v1/spanner_instance_admin.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/keys.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/mutation.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/query_plan.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/result_set.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/spanner.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/transaction.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/spanner/v1/type.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(spanner_protos)
target_link_libraries(
    googleapis_cpp_spanner_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::api_resource_protos
           googleapis-c++::iam_v1_iam_policy_protos
           googleapis-c++::iam_v1_policy_protos
           googleapis-c++::longrunning_operations_protos
           googleapis-c++::rpc_status_protos
    PRIVATE googleapis_cpp_common_flags)

google_cloud_cpp_grpcpp_library(
    googleapis_cpp_storage_protos
    "${GOOGLEAPIS_CPP_SOURCE}/google/storage/v1/storage.proto"
    "${GOOGLEAPIS_CPP_SOURCE}/google/storage/v1/storage_resources.proto"
    PROTO_PATH_DIRECTORIES
    "${GOOGLEAPIS_CPP_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
googleapis_cpp_set_version_and_alias(storage_protos)
target_link_libraries(
    googleapis_cpp_storage_protos
    PUBLIC googleapis-c++::api_annotations_protos
           googleapis-c++::api_client_protos
           googleapis-c++::api_field_behavior_protos
           googleapis-c++::iam_v1_iam_policy_protos
           googleapis-c++::iam_v1_policy_protos
    PRIVATE googleapis_cpp_common_flags)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
include(GNUInstallDirs)

install(
    TARGETS ${googleapis_cpp_installed_libraries_list}
            googleapis_cpp_common_flags
    EXPORT googleapis-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

foreach (target ${googleapis_cpp_installed_libraries_list})
    google_cloud_cpp_install_proto_library_headers("${target}")
    google_cloud_cpp_install_proto_library_protos("${target}"
                                                  "${GOOGLEAPIS_CPP_SOURCE}")
endforeach ()

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT googleapis-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis")

# Use a function to create a scope for the variables.
function (googleapis_cpp_install_pc target)
    string(REPLACE "googleapis_cpp_" "" _short_name ${target})
    string(REPLACE "_protos" "" _short_name ${_short_name})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "The Google APIS C++ ${_short_name} Proto Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Compiled proto for C++.")
    # Examine the target LINK_LIBRARIES property, use that to pull the
    # dependencies between the googleapis-c++::* libraries.
    set(_target_pc_requires)
    get_target_property(_target_deps ${target} LINK_LIBRARIES)
    foreach (dep ${_target_deps})
        if ("${dep}" MATCHES "^googleapis-c\\+\\+::")
            string(REPLACE "googleapis-c++::" "googleapis_cpp_" dep "${dep}")
            list(APPEND _target_pc_requires " " "${dep}")
        endif ()
    endforeach ()
    # These dependencies are required for all the googleapis-c++::* libraries.
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
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction ()

# Create and install the pkg-config files.
foreach (target ${googleapis_cpp_installed_libraries_list})
    googleapis_cpp_install_pc("${target}")
endforeach ()

# Create and install the googleapis pkg-config file for backwards compatibility.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google APIS C++ Proto Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Platforms.")
# Note the use of spaces, `string(JOIN)` is not available in cmake-3.5, so we
# need to add the separator ourselves.
string(
    CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES
           "googleapis_cpp_bigtable_protos"
           " googleapis_cpp_cloud_bigquery_protos"
           " googleapis_iam_protos"
           " googleapis_pubsub_protos"
           " googleapis_cpp_storage_protos"
           " googleapis_cpp_iam_v1_iam_policy_protos"
           " googleapis_cpp_iam_v1_options_protos"
           " googleapis_cpp_iam_v1_policy_protos"
           " googleapis_cpp_longrunning_operations_protos"
           " googleapis_cpp_api_auth_protos"
           " googleapis_cpp_api_annotations_protos"
           " googleapis_cpp_api_client_protos"
           " googleapis_cpp_api_field_behavior_protos"
           " googleapis_cpp_api_http_protos"
           " googleapis_cpp_rpc_status_protos"
           " googleapis_cpp_rpc_error_details_protos"
           " grpc++"
           " grpc"
           " openssl"
           " protobuf"
           " zlib"
           " libcares")
set(GOOGLE_CLOUD_CPP_PC_LIBS "")
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in" "googleapis.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.cmake.in"
               "googleapis-config.cmake" @ONLY)
write_basic_package_version_file(
    "googleapis-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config-version.cmake"
          "${PROJECT_SOURCE_DIR}/cmake/FindgRPC.cmake"
          "${PROJECT_SOURCE_DIR}/cmake/FindProtobufWithTargets.cmake"
          "${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis")
