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
    "https://github.com/googleapis/googleapis/archive/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}.tar.gz"
    "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA}.tar.gz"
)
set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL_HASH
    "${_GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256}")
if (GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL)
    set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL
        ${GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL})
endif ()
if (GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL_HASH)
    set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL_HASH
        "${GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL_HASH}")
endif ()

set(EXTERNAL_GOOGLEAPIS_PROTO_FILES
    # cmake-format: sort
    "google/api/annotations.proto"
    "google/api/auth.proto"
    "google/api/backend.proto"
    "google/api/billing.proto"
    "google/api/client.proto"
    "google/api/config_change.proto"
    "google/api/consumer.proto"
    "google/api/context.proto"
    "google/api/control.proto"
    "google/api/distribution.proto"
    "google/api/documentation.proto"
    "google/api/endpoint.proto"
    "google/api/error_reason.proto"
    "google/api/field_behavior.proto"
    "google/api/http.proto"
    "google/api/httpbody.proto"
    "google/api/label.proto"
    "google/api/launch_stage.proto"
    "google/api/log.proto"
    "google/api/logging.proto"
    "google/api/metric.proto"
    "google/api/monitored_resource.proto"
    "google/api/monitoring.proto"
    "google/api/quota.proto"
    "google/api/resource.proto"
    "google/api/routing.proto"
    "google/api/service.proto"
    "google/api/source_info.proto"
    "google/api/system_parameter.proto"
    "google/api/usage.proto"
    "google/api/visibility.proto"
    # orgpolicy/v**1** is used *indirectly* by google/cloud/asset, therefore it
    # does not appear in protolists/asset.list. In addition, it is not compiled
    # by any other library. So, added manually.
    "google/cloud/orgpolicy/v1/orgpolicy.proto"
    "google/devtools/source/v1/source_context.proto"
    "google/longrunning/operations.proto"
    "google/rpc/code.proto"
    "google/rpc/error_details.proto"
    "google/rpc/status.proto"
    "google/type/calendar_period.proto"
    "google/type/color.proto"
    "google/type/date.proto"
    "google/type/datetime.proto"
    "google/type/dayofweek.proto"
    "google/type/decimal.proto"
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

# Set EXTERNAL_GOOGLEAPIS_SOURCE in the parent directory, as it is used by all
# the generated libraries.  The Conan packages (https://conan.io), will need to
# patch this value.  Setting the value in a single place makes such patching
# easier.
set(EXTERNAL_GOOGLEAPIS_PREFIX "${PROJECT_BINARY_DIR}/external/googleapis")
set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${EXTERNAL_GOOGLEAPIS_PREFIX}/src/googleapis_download"
    PARENT_SCOPE)
set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${EXTERNAL_GOOGLEAPIS_PREFIX}/src/googleapis_download")

# Include the functions to compile proto files and maintain proto libraries.
include(CompileProtos)

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

externalproject_add(
    googleapis_download
    EXCLUDE_FROM_ALL ON
    PREFIX "${EXTERNAL_GOOGLEAPIS_PREFIX}"
    URL ${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL}
    URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL_HASH}
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

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(SelectMSVCRuntime)

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
                   VERSION "${PROJECT_VERSION}"
                   SOVERSION ${PROJECT_VERSION_MAJOR})
    add_library("google-cloud-cpp::${short_name}" ALIAS
                "google_cloud_cpp_${short_name}")
endfunction ()

set(external_googleapis_installed_libraries_list
    # cmake-format: sort
    google_cloud_cpp_bigtable_protos
    google_cloud_cpp_cloud_bigquery_protos
    google_cloud_cpp_cloud_common_common_protos
    google_cloud_cpp_cloud_speech_protos
    google_cloud_cpp_cloud_texttospeech_protos
    google_cloud_cpp_devtools_cloudtrace_v2_trace_protos
    google_cloud_cpp_devtools_cloudtrace_v2_tracing_protos
    google_cloud_cpp_devtools_source_v1_source_context_protos
    google_cloud_cpp_iam_protos
    google_cloud_cpp_iam_v1_iam_policy_protos
    google_cloud_cpp_iam_v1_options_protos
    google_cloud_cpp_iam_v1_policy_protos
    google_cloud_cpp_logging_protos
    google_cloud_cpp_longrunning_operations_protos
    google_cloud_cpp_monitoring_protos
    google_cloud_cpp_pubsub_protos
    google_cloud_cpp_spanner_protos
    google_cloud_cpp_storage_protos)

# These proto files cannot be added in the foreach() loop because they have
# dependencies.
set(PROTO_FILES_WITH_DEPENDENCIES
    # cmake-format: sort
    "google/api/annotations.proto"
    "google/api/auth.proto"
    "google/api/billing.proto"
    "google/api/client.proto"
    "google/api/distribution.proto"
    "google/api/endpoint.proto"
    "google/api/log.proto"
    "google/api/logging.proto"
    "google/api/metric.proto"
    "google/api/monitored_resource.proto"
    "google/api/monitoring.proto"
    "google/api/quota.proto"
    "google/api/service.proto"
    "google/api/usage.proto"
    "google/rpc/status.proto")

# For some directories *most* (but not all) the proto files are simple enough
# that the libraries can be generated with a foreach() loop.
foreach (proto IN LISTS EXTERNAL_GOOGLEAPIS_PROTO_FILES)
    if (proto MATCHES "^google/api/"
        OR proto MATCHES "^google/type"
        OR proto MATCHES "^google/rpc/")
        external_googleapis_short_name(short_name "${proto}")
        list(APPEND external_googleapis_installed_libraries_list
             google_cloud_cpp_${short_name})
        list(FIND PROTO_FILES_WITH_DEPENDENCIES "${proto}" has_dependency)
        if (has_dependency EQUAL -1)
            external_googleapis_add_library("${proto}")
        endif ()
    endif ()
endforeach ()

# Out of order because they have dependencies.
external_googleapis_add_library("google/api/annotations.proto" api_http_protos)
external_googleapis_add_library("google/api/auth.proto" api_annotations_protos)
external_googleapis_add_library("google/api/client.proto"
                                api_launch_stage_protos)
external_googleapis_add_library("google/api/metric.proto"
                                api_launch_stage_protos api_label_protos)
external_googleapis_add_library("google/api/billing.proto"
                                api_annotations_protos api_metric_protos)
external_googleapis_add_library("google/api/distribution.proto"
                                api_annotations_protos)
external_googleapis_add_library("google/api/endpoint.proto"
                                api_annotations_protos)
external_googleapis_add_library("google/api/log.proto" api_label_protos)
external_googleapis_add_library("google/api/logging.proto"
                                api_annotations_protos api_label_protos)
external_googleapis_add_library("google/api/monitored_resource.proto"
                                api_launch_stage_protos api_label_protos)
external_googleapis_add_library("google/api/monitoring.proto"
                                api_annotations_protos)
external_googleapis_add_library("google/api/quota.proto" api_annotations_protos)
external_googleapis_add_library("google/api/usage.proto" api_annotations_protos
                                api_visibility_protos)
external_googleapis_add_library(
    "google/api/service.proto"
    api_annotations_protos
    api_auth_protos
    api_backend_protos
    api_billing_protos
    api_client_protos
    api_context_protos
    api_control_protos
    api_documentation_protos
    api_endpoint_protos
    api_http_protos
    api_label_protos
    api_log_protos
    api_logging_protos
    api_metric_protos
    api_monitored_resource_protos
    api_monitoring_protos
    api_quota_protos
    api_resource_protos
    api_source_info_protos
    api_system_parameter_protos
    api_usage_protos)

external_googleapis_add_library("google/iam/v1/options.proto"
                                api_annotations_protos)
external_googleapis_add_library("google/iam/v1/policy.proto"
                                api_annotations_protos type_expr_protos)
external_googleapis_add_library("google/rpc/status.proto"
                                rpc_error_details_protos)

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

google_cloud_cpp_load_protolist(cloud_common_list "protolists/common.list")
google_cloud_cpp_load_protodeps(cloud_common_deps "protodeps/common.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_cloud_common_common_protos ${cloud_common_list}
    PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(cloud_common_common_protos)
target_link_libraries(google_cloud_cpp_cloud_common_common_protos
                      PUBLIC ${cloud_common_deps})

external_googleapis_add_library(
    "google/devtools/cloudtrace/v2/trace.proto" api_annotations_protos
    api_field_behavior_protos api_resource_protos rpc_status_protos)
external_googleapis_add_library(
    "google/devtools/cloudtrace/v2/tracing.proto"
    devtools_cloudtrace_v2_trace_protos api_annotations_protos
    api_client_protos api_field_behavior_protos rpc_status_protos)

external_googleapis_add_library("google/devtools/source/v1/source_context.proto"
                                api_annotations_protos)

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
    external_googleapis_install_pc("${target}")
endforeach ()

external_googleapis_install_pc_common(google_cloud_cpp_logging_type_protos)
set(GOOGLE_CLOUD_CPP_PC_LIBS "")
google_cloud_cpp_set_pkgconfig_paths()
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.pc.in"
               "google_cloud_cpp_logging_type_protos.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_logging_type_protos.pc"
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
google_cloud_cpp_set_pkgconfig_paths()
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.pc.in"
               "googleapis.pc" @ONLY)
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
        "${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_googleapis"
    COMPONENT google_cloud_cpp_development)
