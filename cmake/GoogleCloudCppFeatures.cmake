# ~~~
# Copyright 2023 Google LLC
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

include(CMakeDependentOption)
include(CreateBazelConfig)

# The default list of libraries to build. These can be overridden by the user by
# passing a comma-separated list, i.e
# `-DGOOGLE_CLOUD_CPP_ENABLE=spanner,storage`.
set(GOOGLE_CLOUD_CPP_LEGACY_FEATURES
    "bigtable;bigquery;iam;logging;pubsub;spanner;storage")

# Note that libraries like `compute` or `sql` are not REST only. Their API uses
# Protobuf messages. We do not bother to have an internal library that depends
# on Protobuf but not gRPC. So these libraries must depend on
# `google_cloud_cpp_grpc_utils`.
set(GOOGLE_CLOUD_CPP_NO_GRPC_FEATURES
    # cmake-format: sort
    "experimental-bigquery_rest" "mocks" "oauth2" "storage")

set(GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES
    # cmake-format: sort
    "pubsublite" # This is WIP, it needs a number of hand-crafted APIs.
)

set(GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES # cmake-format: sort
                                          "storagecontrol")

set(GOOGLE_CLOUD_CPP_GA_LIBRARIES
    # cmake-format: sort
    "accessapproval"
    "accesscontextmanager"
    "advisorynotifications"
    "aiplatform"
    "alloydb"
    "apigateway"
    "apigeeconnect"
    "apikeys"
    "appengine"
    "apphub"
    "artifactregistry"
    "asset"
    "assuredworkloads"
    "automl"
    "backupdr"
    "baremetalsolution"
    "batch"
    "beyondcorp"
    "bigquery"
    "bigtable"
    "billing"
    "binaryauthorization"
    "certificatemanager"
    "channel"
    "cloudbuild"
    "cloudcontrolspartner"
    "cloudquotas"
    "commerce"
    "composer"
    "compute"
    "confidentialcomputing"
    "config"
    "connectors"
    "contactcenterinsights"
    "container"
    "containeranalysis"
    "contentwarehouse"
    "datacatalog"
    "datafusion"
    "datamigration"
    "dataplex"
    "dataproc"
    "datastore"
    "datastream"
    "deploy"
    "dialogflow_cx"
    "dialogflow_es"
    "discoveryengine"
    "dlp"
    "documentai"
    "domains"
    "edgecontainer"
    "edgenetwork"
    "essentialcontacts"
    "eventarc"
    "filestore"
    "functions"
    "gkebackup"
    "gkehub"
    "gkemulticloud"
    "iam"
    "iap"
    "ids"
    "kms"
    "language"
    "logging"
    "managedidentities"
    "memcache"
    "metastore"
    "migrationcenter"
    "monitoring"
    "netapp"
    "networkconnectivity"
    "networkmanagement"
    "networksecurity"
    "networkservices"
    "notebooks"
    "oauth2"
    "optimization"
    "orgpolicy"
    "osconfig"
    "oslogin"
    "policysimulator"
    "policytroubleshooter"
    "privateca"
    "profiler"
    "pubsub"
    "rapidmigrationassessment"
    "recaptchaenterprise"
    "recommender"
    "redis"
    "resourcemanager"
    "resourcesettings"
    "retail"
    "run"
    "scheduler"
    "secretmanager"
    "securesourcemanager"
    "securitycenter"
    "securitycentermanagement"
    "servicecontrol"
    "servicedirectory"
    "servicehealth"
    "servicemanagement"
    "serviceusage"
    "shell"
    "spanner"
    "speech"
    "sql"
    "storage"
    "storageinsights"
    "storagetransfer"
    "support"
    "talent"
    "tasks"
    "telcoautomation"
    "texttospeech"
    "timeseriesinsights"
    "tpu"
    "trace"
    "translate"
    "video"
    "videointelligence"
    "vision"
    "vmmigration"
    "vmwareengine"
    "vpcaccess"
    "webrisk"
    "websecurityscanner"
    "workflows"
    "workstations")

set(GOOGLE_CLOUD_CPP_COMPUTE_LIBRARIES
    # cmake-format: sorted
    "compute_accelerator_types"
    "compute_addresses"
    "compute_autoscalers"
    "compute_backend_buckets"
    "compute_backend_services"
    "compute_disks"
    "compute_disk_types"
    "compute_external_vpn_gateways"
    "compute_firewall_policies"
    "compute_firewalls"
    "compute_forwarding_rules"
    "compute_global_addresses"
    "compute_global_forwarding_rules"
    "compute_global_network_endpoint_groups"
    "compute_global_public_delegated_prefixes"
    "compute_health_checks"
    "compute_http_health_checks"
    "compute_https_health_checks"
    "compute_image_family_views"
    "compute_images"
    "compute_instance_group_managers"
    "compute_instance_groups"
    "compute_instances"
    "compute_instance_templates"
    "compute_interconnect_attachments"
    "compute_interconnect_locations"
    "compute_interconnects"
    "compute_license_codes"
    "compute_licenses"
    "compute_machine_images"
    "compute_machine_types"
    "compute_network_attachments"
    "compute_network_edge_security_services"
    "compute_network_endpoint_groups"
    "compute_network_firewall_policies"
    "compute_networks"
    "compute_node_groups"
    "compute_node_templates"
    "compute_node_types"
    "compute_packet_mirrorings"
    "compute_projects"
    "compute_public_advertised_prefixes"
    "compute_public_delegated_prefixes"
    "compute_region_autoscalers"
    "compute_region_backend_services"
    "compute_region_commitments"
    "compute_region_disks"
    "compute_region_disk_types"
    "compute_region_health_check_services"
    "compute_region_health_checks"
    "compute_region_instance_group_managers"
    "compute_region_instance_groups"
    "compute_region_instances"
    "compute_region_instance_templates"
    "compute_region_network_endpoint_groups"
    "compute_region_network_firewall_policies"
    "compute_region_notification_endpoints"
    "compute_region_security_policies"
    "compute_region_ssl_certificates"
    "compute_ssl_policies"
    "compute_subnetworks"
    "compute_target_grpc_proxies"
    "compute_target_http_proxies"
    "compute_target_https_proxies"
    "compute_target_instances"
    "compute_target_pools"
    "compute_target_ssl_proxies"
    "compute_target_tcp_proxies"
    "compute_target_vpn_gateways"
    "compute_url_maps"
    "compute_vpn_gateways"
    "compute_vpn_tunnels"
    "compute_zones"
    "compute_global_operations"
    "compute_global_organization_operations"
    "compute_region_operations"
    "compute_zone_operations")

# Use a function to get a new scope, so the GOOGLE_CLOUD_CPP_*_LIBRARIES remain
# unchanged.
function (export_libraries_bzl)
    foreach (stage IN ITEMS EXPERIMENTAL TRANSITION GA)
        set(var "GOOGLE_CLOUD_CPP_${stage}_LIBRARIES")
        if (NOT "compute" IN_LIST ${var})
            continue()
        endif ()
        list(REMOVE_ITEM ${var} "compute")
        list(APPEND ${var} ${GOOGLE_CLOUD_CPP_COMPUTE_LIBRARIES})
        list(SORT ${var})
    endforeach ()

    export_list_to_bazel(
        "libraries.bzl" YEAR 2023 GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES
        GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES GOOGLE_CLOUD_CPP_GA_LIBRARIES)
endfunction ()
export_libraries_bzl()

# ~~~
# Handle the dependencies between features. That is, if feature "X" is enabled
# also enable feature "Y" because "X" depends on "Y".
#
# This also implements a few meta-features:
# __ga_libraries__: compile all the GA libraries
# __experimental_libraries__: compile all the experimental libraries
# __common__: compile all the common libraries, even if not needed by any
#   other feature, or even if no features are enabled.
# ~~~
macro (google_cloud_cpp_enable_deps)
    if (__ga_libraries__ IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE ${GOOGLE_CLOUD_CPP_GA_LIBRARIES})
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE
             ${GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES})
    endif ()
    if (__experimental_libraries__ IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE
             ${GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES})
    endif ()
    if (__common__ IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        set(GOOGLE_CLOUD_CPP_ENABLE_GRPC ON)
        set(GOOGLE_CLOUD_CPP_ENABLE_REST ON)
    endif ()
    if (compute IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE
             ${GOOGLE_CLOUD_CPP_COMPUTE_LIBRARIES})
        list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE "compute")
    endif ()
    set(disabled_features ${GOOGLE_CLOUD_CPP_ENABLE})
    list(FILTER disabled_features INCLUDE REGEX "^-")
    foreach (disabled IN LISTS disabled_features)
        if (disabled STREQUAL "-compute")
            list(FILTER GOOGLE_CLOUD_CPP_ENABLE EXCLUDE REGEX "^compute_.*")
            list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE "compute")
            continue()
        endif ()
        string(SUBSTRING "${disabled}" 1 -1 feature)
        list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE "${feature}")
    endforeach ()

    list(FILTER GOOGLE_CLOUD_CPP_ENABLE EXCLUDE REGEX "^-")
    list(FILTER GOOGLE_CLOUD_CPP_ENABLE EXCLUDE REGEX "^__")

    if (asset IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 accesscontextmanager osconfig)
    endif ()
    if (contentwarehouse IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 documentai)
    endif ()
    if (pubsublite IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 pubsub)
    endif ()
    if (pubsub IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 iam)
    endif ()
    if ((storage_grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        # TODO(#13857) - remove backwards compatibility shims
        OR (experimental-storage_grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 storage)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 opentelemetry)
    endif ()
    if (opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 monitoring trace)
    endif ()
endmacro ()

# Cleanup the "GOOGLE_CLOUD_CPP_ENABLE" variable. Remove duplicates, and set
# backwards compatibility flags.
macro (google_cloud_cpp_enable_cleanup)
    # Remove any library that's been disabled from the list.
    foreach (library IN LISTS GOOGLE_CLOUD_CPP_LEGACY_FEATURES)
        string(TOUPPER "GOOGLE_CLOUD_CPP_ENABLE_${library}" feature_flag)
        if ("${library}" IN_LIST GOOGLE_CLOUD_CPP_ENABLE
            AND NOT "${${feature_flag}}")
            message(
                WARNING "Using ${feature_flag} is discouraged. Please use the"
                        " unified GOOGLE_CLOUD_CPP_ENABLE list instead."
                        " The ${library} feature will not be built as"
                        " ${feature_flag} is turned off.")
            list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE ${library})
        endif ()
    endforeach ()

    list(REMOVE_DUPLICATES GOOGLE_CLOUD_CPP_ENABLE)

    set(grpc_features ${GOOGLE_CLOUD_CPP_ENABLE})
    list(REMOVE_ITEM grpc_features ${GOOGLE_CLOUD_CPP_NO_GRPC_FEATURES})
    if (grpc_features)
        set(GOOGLE_CLOUD_CPP_ENABLE_GRPC ON)
    endif ()

    set(compute_features ${GOOGLE_CLOUD_CPP_ENABLE})
    list(FILTER compute_features INCLUDE REGEX "^compute_.*")
    if ((storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (compute IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (compute_features)
        OR (experimental-bigquery_rest IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (oauth2 IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (sql IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (universe_domain IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (generator IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
        set(GOOGLE_CLOUD_CPP_ENABLE_REST ON)
    endif ()

    list(REMOVE_DUPLICATES GOOGLE_CLOUD_CPP_ENABLE)
endmacro ()

# Configure CMake to build the features listed in `GOOGLE_CLOUD_CPP_ENABLE`.
# Most of them are subdirectories in `google/cloud/`. Some number of them have
# additional samples that are enabled if needed.
function (google_cloud_cpp_enable_features)
    set(compute_added FALSE)
    foreach (feature IN LISTS GOOGLE_CLOUD_CPP_ENABLE)
        if ("${feature}" STREQUAL "generator")
            add_subdirectory(generator)
        elseif (("${feature}" STREQUAL "storage_grpc")
                # TODO(#13857) - remove backwards compatibility shims
                OR ("${feature}" STREQUAL "experimental-storage_grpc"))
            if (NOT ("storage" IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
                add_subdirectory(google/cloud/storage)
            endif ()
        elseif ("${feature}" STREQUAL "experimental-bigquery_rest")
            if (NOT ("bigquery" IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
                add_subdirectory(google/cloud/bigquery)
            endif ()
        elseif ("${feature}" STREQUAL "experimental-http-transcoding")
            continue()
        elseif ("${feature}" STREQUAL "universe_domain")
            continue()
        elseif ("${feature}" MATCHES "^compute_.*")
            if (NOT compute_added)
                add_subdirectory(protos/google/cloud/compute)
                add_subdirectory(google/cloud/compute)
                set(compute_added TRUE)
            endif ()
        else ()
            if (NOT IS_DIRECTORY
                "${CMAKE_CURRENT_SOURCE_DIR}/google/cloud/${feature}"
                OR NOT
                   EXISTS
                   "${CMAKE_CURRENT_SOURCE_DIR}/google/cloud/${feature}/CMakeLists.txt"
            )
                message(
                    WARNING
                        "${feature} is not a valid feature in google-cloud-cpp, ignored"
                )
                continue()
            endif ()
            add_subdirectory(google/cloud/${feature})
            if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES
                AND IS_DIRECTORY
                    "${CMAKE_CURRENT_SOURCE_DIR}/google/cloud/${feature}/samples"
                AND EXISTS
                    "${CMAKE_CURRENT_SOURCE_DIR}/google/cloud/${feature}/samples/CMakeLists.txt"
            )
                add_subdirectory(google/cloud/${feature}/samples)
            endif ()
            # Older libraries used examples/ to host their examples. The
            # directory name cannot be easily changed as cloud.google.com
            # references these files explicitly.
            if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES
                AND IS_DIRECTORY
                    "${CMAKE_CURRENT_SOURCE_DIR}/google/cloud/${feature}/examples"
            )
                add_subdirectory(google/cloud/${feature}/examples)
            endif ()
        endif ()
    endforeach ()
endfunction ()

function (google_cloud_cpp_define_legacy_feature_options)
    # These per-feature flags are here just for backwards compatibility. We no
    # longer recommend their usage. Build scripts should just use
    # GOOGLE_CLOUD_CPP_ENABLE
    option(
        GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE
        "Enable building the Bigtable library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY
        "Enable building the Bigquery library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_SPANNER
        "Enable building the Spanner library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_STORAGE
        "Enable building the Storage library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_PUBSUB
        "Enable building the Pub/Sub library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_IAM
        "Enable building the IAM library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_LOGGING
        "Enable building the Logging library. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        ON)
    option(
        GOOGLE_CLOUD_CPP_ENABLE_GENERATOR
        "Enable building the generator. Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        OFF)
    # Since these are not the recommended practice, mark them as advanced.
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_SPANNER)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_IAM)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_LOGGING)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)
endfunction ()

function (google_cloud_cpp_define_dependent_legacy_feature_options)
    set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT OFF)
    if ((storage_grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        # TODO(#13857) - remove backwards compatibility shims
        OR (experimental-storage_grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
        set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT ON)
    endif ()
    option(
        GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC
        "Enable compilation for the GCS gRPC plugin (EXPERIMENTAL).  Deprecated, prefer GOOGLE_CLOUD_CPP_ENABLE."
        "${GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT}")
    mark_as_advanced(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

    option(
        GOOGLE_CLOUD_CPP_ENABLE_GRPC
        "Enable building the gRPC utilities library. This is now unused, the grpc_utils library is only enabled if GOOGLE_CLOUD_CPP_ENABLE contains any feature that requires it."
        OFF)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GRPC)

    # The only case where REST support is not needed is when not compiling
    # services with REST interfaces.
    option(
        GOOGLE_CLOUD_CPP_ENABLE_REST
        "Enable building the REST transport library. This is now unused, the rest_internal library is only enabled if GOOGLE_CLOUD_CPP_ENABLE contains any feature that requires it."
        OFF)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_REST)
endfunction ()

macro (google_cloud_cpp_apply_legacy_feature_options)
    # We no longer build the generator by default, but if it was explicitly
    # requested, we add it to the list of enabled libraries.
    if (GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE "generator")
    endif ()
endmacro ()
