#!/usr/bin/env bash
#
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

# This bash library includes functions to shard CI builds.
#
# Some builds are too slow (`clang-tidy.sh` is a particularly good example).
# We need to use multiple Google Cloud Build invocations for these builds
# to complete quickly enough. This library provides lists of approximately
# balanced groups of features.
#
# Example:
#
#   source module /ci/lib/shard.sh
#   mapfile -t features < <(shard::features $my_shard)

# Make our include guard clean against set -o nounset.
test -n "${CI_LIB_SHARD_SH__:-}" || declare -i CI_LIB_SHARD_SH__=0
if ((CI_LIB_SHARD_SH__++ != 0)); then
  return 0
fi # include guard

readonly DEFAULT_SHARD=(
  bigtable
  spanner
  logging
)

readonly BIGQUERY_SHARD=(
  bigquery
  experimental-bigquery_rest
)

readonly PUBSUB_SHARD=(
  pubsub
  # Pub/Sub Lite is included because it has hand-crafted code and requires
  # Pub/Sub
  pubsublite
  # IAM is included because it has hand-crafted tests and/or examples and is
  # required by Pub/Sub
  iam
)

readonly STORAGE_SHARD=(
  storage
  storagecontrol
  experimental-storage_grpc
  # oauth2 and universe_domain are included because their deps are already built
  # by storage.
  oauth2
  universe_domain
)

readonly TOOLS_SHARD=(
  generator
  docfx
)

function shard::cmake_features() {
  case "$1" in
    __default__)
      printf "%s\n" "${DEFAULT_SHARD[@]}" "opentelemetry"
      ;;
    bigquery)
      printf "%s\n" "${BIGQUERY_SHARD[@]}" "opentelemetry"
      ;;
    pubsub)
      printf "%s\n" "${PUBSUB_SHARD[@]}" "opentelemetry"
      ;;
    storage)
      printf "%s\n" "${STORAGE_SHARD[@]}" "opentelemetry"
      ;;
    __tools__)
      printf "%s\n" "${TOOLS_SHARD[@]}"
      ;;
    compute)
      echo "compute"
      echo "opentelemetry"
      ;;
    # If we decided to compile all libraries with clang-tidy we would need
    # to enable shards such as:
    __other__)
      echo "__ga_libraries__"
      echo "opentelemetry"
      printf "-%s\n" "${DEFAULT_SHARD[@]}"
      printf "-%s\n" "${BIGQUERY_SHARD[@]}"
      printf "-%s\n" "${PUBSUB_SHARD[@]}"
      printf "-%s\n" "${STORAGE_SHARD[@]}"
      echo "-compute"
      printf "-%s\n" "${TOOLS_SHARD[@]}"
      ;;
    *)
      echo "$1"
      echo "opentelemetry"
      ;;
  esac
}
