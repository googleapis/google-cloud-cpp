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

# The default list of libraries to build. These can be overridden by the user by
# passing a comma-separated list, i.e
# `-DGOOGLE_CLOUD_CPP_ENABLE=spanner,storage`.
set(GOOGLE_CLOUD_CPP_LEGACY_FEATURES
    "bigtable;bigquery;iam;logging;pubsub;spanner;storage")

# Handle the dependencies between features. That is, if feature "X" is enabled
# also enable feature "Y" because "X" depends on "Y".
function (google_cloud_cpp_enable_deps)
    if (asset IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE accesscontextmanager osconfig)
    endif ()
    if (binaryauthorization IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE grafeas)
    endif ()
    if (containeranalysis IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE grafeas)
    endif ()
    if (pubsublite IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE pubsub)
    endif ()
    if (experimental-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
        list(APPEND GOOGLE_CLOUD_CPP_ENABLE storage)
    endif ()
    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
        PARENT_SCOPE)
endfunction ()

# Cleanup the "GOOGLE_CLOUD_CPP_ENABLE" variable. Remove duplicates, and set
# backwards compatibility flags.
function (google_cloud_cpp_enable_cleanup)
    # Remove any library that's been disabled from the GOOGLE_CLOUD_CPP_ENABLE
    # list.
    foreach (library IN LISTS GOOGLE_CLOUD_CPP_LEGACY_FEATURES)
        string(TOUPPER "GOOGLE_CLOUD_CPP_ENABLE_${library}" feature_flag)
        message("feature_flag=${feature_flag} IS ${${feature_flag}}")
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
    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
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
        elseif ("${feature}" STREQUAL "experimental-opentelemetry")

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

# These per-feature flags are here just for backwards compatibility. We no
# longer recommend their usage. Build scripts should just use
# GOOGLE_CLOUD_CPP_ENABLE
option(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE "Enable building the Bigtable library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY "Enable building the Bigquery library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_SPANNER "Enable building the Spanner library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_STORAGE "Enable building the Storage library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB "Enable building the Pub/Sub library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_IAM "Enable building the IAM library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_LOGGING "Enable building the Logging library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_GENERATOR "Enable building the generator." OFF)
# Since these are not the recommended practice, mark them as advanced.
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_SPANNER)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_IAM)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_LOGGING)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)

set(GOOGLE_CLOUD_CPP_ENABLE
    ${GOOGLE_CLOUD_CPP_LEGACY_FEATURES}
    CACHE STRING "The list of libraries to build.")
string(REPLACE "," ";" GOOGLE_CLOUD_CPP_ENABLE "${GOOGLE_CLOUD_CPP_ENABLE}")

set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT OFF)
if (experimental-storage-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT ON)
endif ()
option(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC
       "Enable compilation for the GCS gRPC plugin (EXPERIMENTAL)"
       "${GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT}")
mark_as_advanced(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

# We no longer build the generator by default, but if it was explicitly
# requested, we add it to the list of enabled libraries.
if (GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)
    list(APPEND GOOGLE_CLOUD_CPP_ENABLE "generator")
endif ()

google_cloud_cpp_enable_deps()
google_cloud_cpp_enable_cleanup()

if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC AND NOT
                                             GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    message(
        FATAL_ERROR
            "The experimental GCS+gRPC plugin is enabled, but the base storage"
            " library is disabled. This is not a valid configuration, please"
            " review the options provided to CMake. Pay particular attention to"
            " GOOGLE_CLOUD_CPP_ENABLE, GOOGLE_CLOUD_CPP_ENABLE_STORAGE, and"
            " GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC.")
endif ()

# The only case where gRPC support is not needed is when compiling *only* the
# storage library.
set(GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION ON)
if ("${GOOGLE_CLOUD_CPP_ENABLE}" STREQUAL "storage")
    set(GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION OFF)
endif ()

cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_GRPC "Enable building the gRPC utilities library."
    ON "GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GRPC)

# The only case where REST support is not needed is when not compiling services
# with REST interfaces.
set(GOOGLE_CLOUD_CPP_ENABLE_REST_EXPRESSION ON)
if (NOT storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND NOT experimental-storage-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND NOT generator IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    set(GOOGLE_CLOUD_CPP_ENABLE_REST_EXPRESSION OFF)
endif ()
cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_REST "Enable building the REST transport library."
    ON "GOOGLE_CLOUD_CPP_ENABLE_REST_EXPRESSION" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_REST)

# Detect incompatible flags and let the customer know what to do.
if (storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE AND NOT
                                                GOOGLE_CLOUD_CPP_ENABLE_REST)
    message(
        FATAL_ERROR
            "If storage is enabled (e.g. -DGOOGLE_CLOUD_CPP_ENABLE=storage),"
            " the REST library cannot be disabled,"
            " i.e., you cannot set -DGOOGLE_CLOUD_CPP_ENABLE_REST=OFF."
            " If you do want to use the storage library,"
            " then do not use -DGOOGLE_CLOUD_CPP_ENABLE_REST=OFF."
            " If you do not want to use the storage library,"
            " then provide a -DGOOGLE_CLOUD_CPP_ENABLE=... list and do not"
            " include storage in that list.")
endif ()
