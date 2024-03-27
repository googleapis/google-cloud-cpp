#!/bin/bash
#
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

# Helper functions to obtain the full set of features in the project.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__++ != 0)); then
  return 0
fi # include guard

function features::always_build() {
  local list
  list=(
    # These have hand-crafted code, therefore we always want to build them.
    bigtable
    experimental-bigquery_rest
    spanner
    storage
    pubsub
    pubsublite
    # While these libraries are automatically generated, they contain
    # hand-crafted tests.
    bigquery
    iam
    logging
    # Build our code with OpenTelemetry. This feature includes both the client
    # library instrumentation, and the GCP exporters.
    opentelemetry
    # Enable storage_grpc in most builds.
    experimental-storage_grpc
  )
  printf "%s\n" "${list[@]}" | sort -u
}

function features::always_build_cmake() {
  local features
  mapfile -t feature_list < <(features::always_build)
  features="$(printf ",%s" "${feature_list[@]}")"
  echo "${features:1}"
}

function features::libraries() {
  local feature_list
  mapfile -t feature_list < <(cmake -P cmake/print-all-features.cmake 2>&1)
  printf "%s\n" "${feature_list[@]}" | sort -u
}

function features::_internal_extra() {
  local list=(
    experimental-bigquery_rest
    opentelemetry
  )
  printf "%s\n" "${list[@]}"
}

function features::list_full() {
  local feature_list
  mapfile -t feature_list < <(features::libraries)
  local extra_list
  mapfile -t extra_list < <(features::_internal_extra)
  printf "%s\n" "${feature_list[@]}" "${extra_list[@]}" | sort -u
}

function features::list_full_cmake() {
  local feature_list
  mapfile -t feature_list < <(features::list_full)
  local concat
  concat="$(printf ",%s" "${feature_list[@]}" "${extra_list[@]}")"
  echo "${concat:1}"
}
