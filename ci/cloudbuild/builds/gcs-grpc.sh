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

set -eu

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/integration.sh

export CC=clang
export CXX=clang++

excluded_rules=(
  "-//google/cloud/storage/examples:storage_service_account_samples"
  "-//google/cloud/storage/tests:service_account_integration_test"
  "-//google/cloud/storage/tests:grpc_integration_test"
)

mapfile -t args < <(bazel::common_args)
# Run as many of the integration tests as possible using production and gRPC:
# "media" says to use the hybrid gRPC/REST client. For more details see
# https://github.com/googleapis/google-cloud-cpp/issues/6268
readonly GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG=media
mapfile -t integration_args < <(integration::bazel_args)
bazel test "${args[@]}" "${integration_args[@]}" -- //google/cloud/storage/... "${excluded_rules[@]}"
