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

set(GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES
    # cmake-format: sorted
    "compute"
    # This is WIP, it needs a number of hand-crafted APIs.
    "pubsublite" "sql")

set(GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES # cmake-format: sorted
    # Transitioned circa 2023-02-22
    "apikeys")

set(GOOGLE_CLOUD_CPP_GA_LIBRARIES
    # cmake-format: sorted
    "accessapproval"
    "accesscontextmanager"
    "advisorynotifications"
    "aiplatform"
    "alloydb"
    "apigateway"
    "apigeeconnect"
    "appengine"
    "artifactregistry"
    "asset"
    "assuredworkloads"
    "automl"
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
    "composer"
    "confidentialcomputing"
    "connectors"
    "contactcenterinsights"
    "container"
    "containeranalysis"
    "datacatalog"
    "datamigration"
    "dataplex"
    "dataproc"
    "datastream"
    "deploy"
    "dialogflow_cx"
    "dialogflow_es"
    "dlp"
    "documentai"
    "domains"
    "edgecontainer"
    "essentialcontacts"
    "eventarc"
    "filestore"
    "functions"
    "gameservices"
    "gkehub"
    "gkemulticloud"
    "iam"
    "iap"
    "ids"
    "iot"
    "kms"
    "language"
    "logging"
    "managedidentities"
    "memcache"
    "monitoring"
    "networkconnectivity"
    "networkmanagement"
    "networkservices"
    "notebooks"
    "optimization"
    "orgpolicy"
    "osconfig"
    "oslogin"
    "policytroubleshooter"
    "privateca"
    "profiler"
    "pubsub"
    "recommender"
    "redis"
    "resourcemanager"
    "resourcesettings"
    "retail"
    "run"
    "scheduler"
    "secretmanager"
    "securitycenter"
    "servicecontrol"
    "servicedirectory"
    "servicemanagement"
    "serviceusage"
    "shell"
    "spanner"
    "speech"
    "storage"
    "storageinsights"
    "storagetransfer"
    "support"
    "talent"
    "tasks"
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

export_list_to_bazel(
    "libraries.bzl" YEAR 2023 GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES
    GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES GOOGLE_CLOUD_CPP_GA_LIBRARIES)

# Handle the dependencies between features. That is, if feature "X" is enabled
# also enable feature "Y" because "X" depends on "Y".
function (google_cloud_cpp_enable_deps)
    if (__ga_libraries__ IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE __ga_libraries__)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE ${GOOGLE_CLOUD_CPP_GA_LIBRARIES})
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE
             ${GOOGLE_CLOUD_CPP_TRANSITION_LIBRARIES})
    endif ()
    if (__experimental_libraries__ IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE __experimental_libraries__)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE
             ${GOOGLE_CLOUD_CPP_EXPERIMENTAL_LIBRARIES})
    endif ()
    if (asset IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 accesscontextmanager osconfig)
    endif ()
    if (binaryauthorization IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 grafeas)
    endif ()
    if (containeranalysis IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 grafeas)
    endif ()
    if (pubsublite IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 pubsub)
    endif ()
    if (pubsub IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 iam)
    endif ()
    if (experimental-storage-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 storage)
    endif ()
    if (experimental-opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(INSERT GOOGLE_CLOUD_CPP_ENABLE 0 trace)
    endif ()
    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
        PARENT_SCOPE)
endfunction ()

# Cleanup the "GOOGLE_CLOUD_CPP_ENABLE" variable. Remove duplicates, and set
# backwards compatibility flags.
function (google_cloud_cpp_enable_cleanup)
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

    set(GOOGLE_CLOUD_CPP_ENABLE_GRPC ON)
    if ("${GOOGLE_CLOUD_CPP_ENABLE}" STREQUAL "storage")
        set(GOOGLE_CLOUD_CPP_ENABLE_GRPC OFF)
    endif ()

    set(GOOGLE_CLOUD_CPP_ENABLE_REST OFF)
    if ((storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (experimental-bigquery_rest IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (experimental-opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (sql IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        OR (generator IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
        set(GOOGLE_CLOUD_CPP_ENABLE_REST ON)
    endif ()

    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
        PARENT_SCOPE)
    set(GOOGLE_CLOUD_CPP_ENABLE_GRPC
        "${GOOGLE_CLOUD_CPP_ENABLE_GRPC}"
        PARENT_SCOPE)
    set(GOOGLE_CLOUD_CPP_ENABLE_REST
        "${GOOGLE_CLOUD_CPP_ENABLE_REST}"
        PARENT_SCOPE)
endfunction ()

# Configure CMake to build the features listed in `GOOGLE_CLOUD_CPP_ENABLE`.
# Most of them are subdirectories in `google/cloud/`. Some number of them have
# additional samples that are enabled if needed.
function (google_cloud_cpp_enable_features)
    foreach (feature ${GOOGLE_CLOUD_CPP_ENABLE})
        if ("${feature}" STREQUAL "generator")
            add_subdirectory(generator)
        elseif ("${feature}" STREQUAL "experimental-storage-grpc")
            if (NOT ("storage" IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
                add_subdirectory(google/cloud/storage)
            endif ()
        elseif ("${feature}" STREQUAL "experimental-bigquery_rest")
            if (NOT ("bigquery" IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
                add_subdirectory(google/cloud/bigquery)
            endif ()
        elseif ("${feature}" STREQUAL "experimental-http-transcoding")
            continue()
        elseif ("${feature}" STREQUAL "experimental-opentelemetry")
            add_subdirectory(google/cloud/opentelemetry)
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

    set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT OFF)
    if (experimental-storage-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
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

function (google_cloud_cpp_apply_legacy_feature_options)
    # We no longer build the generator by default, but if it was explicitly
    # requested, we add it to the list of enabled libraries.
    if (GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE "generator")
    endif ()
    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
        PARENT_SCOPE)
endfunction ()
