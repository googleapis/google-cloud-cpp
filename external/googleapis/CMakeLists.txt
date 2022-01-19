# ~~~
# Copyright 2020 Google LLC
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
# ~~~

include(GoogleapisConfig)

set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL
    "https://github.com/googleapis/googleapis/archive/${GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}.tar.gz"
)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")

set(EXTERNAL_GOOGLEAPIS_PROTO_FILES
    "google/api/http.proto"
    "google/api/httpbody.proto"
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
    "google/rpc/code.proto"
    "google/rpc/error_details.proto"
    "google/rpc/status.proto"
    "google/iam/v1/options.proto"
    "google/iam/v1/policy.proto"
    "google/iam/v1/iam_policy.proto"
    "google/longrunning/operations.proto"
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

function (google_cloud_cpp_load_protolist var file)
    file(READ "${file}" contents)
    string(REGEX REPLACE "\n" ";" contents "${contents}")
    set(protos)
    foreach (line IN LISTS contents)
        string(REPLACE "@com_google_googleapis//" "" line "${line}")
        string(REPLACE ":" "/" line "${line}")
        if ("${line}" STREQUAL "")
            continue()
        endif ()
        list(APPEND protos "${EXTERNAL_GOOGLEAPIS_SOURCE}/${line}")
    endforeach ()
    set(${var}
        "${protos}"
        PARENT_SCOPE)
endfunction ()

function (google_cloud_cpp_load_protodeps var file)
    file(READ "${file}" contents)
    string(REGEX REPLACE "\n" ";" contents "${contents}")
    set(deps)
    foreach (line IN LISTS contents)
        if ("${line}" STREQUAL "")
            continue()
        endif ()
        string(REPLACE ":" "_" line "${line}")
        string(REPLACE "_proto" "_protos" line "${line}")
        string(REPLACE "@com_google_googleapis//" "google-cloud-cpp::" line
                       "${line}")
        string(REPLACE "google-cloud-cpp::google/" "google-cloud-cpp::" line
                       "${line}")
        string(REPLACE "/" "_" line "${line}")
        list(APPEND deps "${line}")
    endforeach ()
    set(${var}
        "${deps}"
        PARENT_SCOPE)
endfunction ()

include(EnableWerror)

set(EXTERNAL_GOOGLEAPIS_BYPRODUCTS)
foreach (proto ${EXTERNAL_GOOGLEAPIS_PROTO_FILES})
    list(APPEND EXTERNAL_GOOGLEAPIS_BYPRODUCTS
         "${EXTERNAL_GOOGLEAPIS_SOURCE}/${proto}")
endforeach ()

file(GLOB protolists "protolists/*.list")
foreach (file IN LISTS protolists)
    google_cloud_cpp_load_protolist(protos "${file}")
    foreach (proto IN LISTS protos)
        list(APPEND EXTERNAL_GOOGLEAPIS_BYPRODUCTS "${proto}")
    endforeach ()
endforeach ()

include(ExternalProject)

ExternalProject_Add(
    googleapis_download
    EXCLUDE_FROM_ALL ON
    PREFIX "${PROJECT_BINARY_DIR}/external/googleapis"
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
    google_cloud_cpp_api_httpbody_protos
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
    google_cloud_cpp_logging_protos
    google_cloud_cpp_monitoring_protos
    google_cloud_cpp_iam_v1_options_protos
    google_cloud_cpp_iam_v1_policy_protos
    google_cloud_cpp_iam_v1_iam_policy_protos
    google_cloud_cpp_rpc_code_protos
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
external_googleapis_add_library("google/api/httpbody.proto")
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

external_googleapis_add_library("google/rpc/code.proto")
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

google_cloud_cpp_load_protolist(cloud_bigquery_list "protolists/bigquery.list")
google_cloud_cpp_load_protodeps(cloud_bigquery_deps "protodeps/bigquery.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_bigquery_protos ${cloud_bigquery_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_bigquery_protos)
target_link_libraries(google_cloud_cpp_cloud_bigquery_protos
                      PUBLIC ${cloud_bigquery_deps})

google_cloud_cpp_load_protolist(bigtable_list "protolists/bigtable.list")
google_cloud_cpp_load_protodeps(bigtable_deps "protodeps/bigtable.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_bigtable_protos ${bigtable_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(bigtable_protos)
target_link_libraries(google_cloud_cpp_bigtable_protos PUBLIC ${bigtable_deps})

google_cloud_cpp_load_protolist(cloud_dialogflow_v2_list
                                "protolists/dialogflow.list")
google_cloud_cpp_load_protodeps(cloud_dialogflow_v2_deps
                                "protodeps/dialogflow.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_dialogflow_v2_protos ${cloud_dialogflow_v2_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_dialogflow_v2_protos)
target_link_libraries(google_cloud_cpp_cloud_dialogflow_v2_protos
                      PUBLIC ${cloud_dialogflow_v2_deps})

list(APPEND external_googleapis_installed_libraries_list
     google_cloud_cpp_cloud_dialogflow_v2_protos)

google_cloud_cpp_load_protolist(cloud_speech_list "protolists/speech.list")
google_cloud_cpp_load_protodeps(cloud_speech_deps "protodeps/speech.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_speech_protos ${cloud_speech_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_speech_protos)
target_link_libraries(google_cloud_cpp_cloud_speech_protos
                      PUBLIC ${cloud_speech_deps})

google_cloud_cpp_load_protolist(cloud_texttospeech_list
                                "protolists/texttospeech.list")
google_cloud_cpp_load_protodeps(cloud_texttospeech_deps
                                "protodeps/texttospeech.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_texttospeech_protos ${cloud_texttospeech_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_texttospeech_protos)
target_link_libraries(google_cloud_cpp_cloud_texttospeech_protos
                      PUBLIC ${cloud_texttospeech_deps})

google_cloud_cpp_load_protolist(iam_list "protolists/iam.list")
google_cloud_cpp_load_protodeps(iam_deps "protodeps/iam.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_iam_protos ${iam_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(iam_protos)
target_link_libraries(google_cloud_cpp_iam_protos PUBLIC ${iam_deps})

google_cloud_cpp_load_protolist(logging_type_list
                                "protolists/logging_type.list")
google_cloud_cpp_load_protodeps(logging_type_deps "protodeps/logging_type.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_logging_type_type_protos ${logging_type_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(logging_type_type_protos)
target_link_libraries(google_cloud_cpp_logging_type_type_protos
                      PUBLIC ${logging_type_deps})

google_cloud_cpp_load_protolist(logging_list "protolists/logging.list")
google_cloud_cpp_load_protodeps(logging_deps "protodeps/logging.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_logging_protos ${logging_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(logging_protos)
target_link_libraries(google_cloud_cpp_logging_protos PUBLIC ${logging_deps})

# This is just for backwards compatibility.
add_library(google_cloud_cpp_logging_type_protos INTERFACE)
set_target_properties(
    "google_cloud_cpp_logging_type_protos"
    PROPERTIES EXPORT_NAME google-cloud-cpp::logging_type_protos)
target_link_libraries(google_cloud_cpp_logging_type_protos
                      INTERFACE google-cloud-cpp::logging_type_type_protos)

google_cloud_cpp_load_protolist(monitoring_list "protolists/monitoring.list")
google_cloud_cpp_load_protodeps(monitoring_deps "protodeps/monitoring.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_monitoring_protos ${monitoring_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(monitoring_protos)
target_link_libraries(google_cloud_cpp_monitoring_protos
                      PUBLIC ${monitoring_deps})

google_cloud_cpp_load_protolist(pubsub_list "protolists/pubsub.list")
google_cloud_cpp_load_protodeps(pubsub_deps "protodeps/pubsub.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_pubsub_protos ${pubsub_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(pubsub_protos)
target_link_libraries(google_cloud_cpp_pubsub_protos PUBLIC ${pubsub_deps})

google_cloud_cpp_load_protolist(spanner_list "protolists/spanner.list")
google_cloud_cpp_load_protodeps(spanner_deps "protodeps/spanner.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_spanner_protos ${spanner_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(spanner_protos)
target_link_libraries(google_cloud_cpp_spanner_protos PUBLIC ${spanner_deps})

google_cloud_cpp_load_protolist(storage_list "protolists/storage.list")
google_cloud_cpp_load_protodeps(storage_deps "protodeps/storage.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_storage_protos ${storage_list} PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}" "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(storage_protos)
target_link_libraries(google_cloud_cpp_storage_protos PUBLIC ${storage_deps})

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
include(GNUInstallDirs)

install(
    TARGETS ${external_googleapis_installed_libraries_list}
            google_cloud_cpp_logging_type_type_protos
            google_cloud_cpp_logging_type_protos
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
            google_cloud_cpp_logging_type_type_protos
            google_cloud_cpp_logging_type_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

foreach (target ${external_googleapis_installed_libraries_list}
                google_cloud_cpp_logging_type_type_protos)
    google_cloud_cpp_install_proto_library_headers("${target}")
    google_cloud_cpp_install_proto_library_protos(
        "${target}" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
endforeach ()

# Create and install the pkg-config files.
foreach (target ${external_googleapis_installed_libraries_list}
                google_cloud_cpp_logging_type_type_protos)
    external_googleapis_install_pc("${target}" "${CMAKE_CURRENT_LIST_DIR}")
    # TODO(#5726) - also install the module with the legacy name
    external_googleapis_install_legacy_pc("${target}"
                                          "${CMAKE_CURRENT_LIST_DIR}")
endforeach ()

external_googleapis_install_pc_common(google_cloud_cpp_logging_type_protos)
set(GOOGLE_CLOUD_CPP_PC_LIBS "")
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in"
               "google_cloud_cpp_logging_type_protos.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_logging_type_protos.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)
# TODO(#5726) - also install the module with the legacy name
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.pc.in"
               "googleapis_cpp_logging_type_protos.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis_cpp_logging_type_protos.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the googleapis pkg-config file for backwards compatibility.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google APIS C++ Proto Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Platforms.")
# Note the use of spaces, `string(JOIN)` is not available in cmake-3.5, so we
# need to add the separator ourselves.
#
# This list is for backwards compatibility purposes only. DO NOT add new
# libraries to it.
#
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
           " google_cloud_cpp_type_expr_protos"
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
