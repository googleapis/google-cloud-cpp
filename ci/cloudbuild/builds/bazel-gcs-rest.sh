#!/bin/bash
#
# Copyright 2022 Google LLC
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
source module ci/cloudbuild/builds/lib/integration.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
bazel test "${args[@]}" --per_file_copt=google/cloud/storage/oauth2/.*\.*@-DGOOGLE_CLOUD_CPP_STORAGE_OAUTH2_HAVE_REST --test_tag_filters=-integration-test ...

mapfile -t integration_args < <(integration::bazel_args)
integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"
