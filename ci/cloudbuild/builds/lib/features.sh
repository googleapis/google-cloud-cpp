#!/bin/bash
#
# Copyright 2021 Google LLC
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

# Helper functions to obtain the full set of features in the project.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_FEATURES_SH__++ != 0)); then
  return 0
fi # include guard

function features::libraries() {
  local feature_list
  mapfile -t feature_list <ci/etc/full_feature_list
  # Add the hand-crafted libraries
  feature_list+=(
    bigtable
    pubsub
    spanner
    storage
  )
  printf "%s\n" "${feature_list[@]}"
}

function features::list() {
  local feature_list
  mapfile -t feature_list < <(features::libraries)
  # Add any custom features to the full list of libraries
  feature_list+=(
    experimental-storage-grpc
  )
  printf "%s\n" "${feature_list[@]}"
}

function features::cmake_definition() {
  local features
  mapfile -t feature_list < <(features::list)
  features="$(printf ",%s" "${feature_list[@]}")"
  echo "${features:1}"
}
