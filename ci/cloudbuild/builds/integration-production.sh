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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/git.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
bazel test "${args[@]}" --test_tag_filters=-integration-test ...

excluded_rules=(
  "-//examples:grpc_credential_types"
  "-//google/cloud/bigtable/examples:bigtable_grpc_credentials"
  "-//google/cloud/storage/examples:storage_service_account_samples"
  "-//google/cloud/storage/tests:service_account_integration_test"
  "-//google/cloud/storage/tests:grpc_integration_test"
)

io::log_h2 "Running the integration tests against prod"
mapfile -t integration_args < <(integration::bazel_args)
io::run bazel test "${args[@]}" "${integration_args[@]}" \
  --cache_test_results="auto" \
  --test_tag_filters="integration-test" -- ... "${excluded_rules[@]}"
