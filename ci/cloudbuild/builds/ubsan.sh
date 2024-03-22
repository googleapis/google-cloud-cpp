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
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

# With UBSAN builds we need to ignore the `quickstart` and demo programs, see:
#   https://github.com/bazelbuild/bazel/issues/11122
deleted_packages="$(find google/cloud -type d -name quickstart -o -name demo | xargs printf ",%s")"

mapfile -t args < <(bazel::common_args)
args+=("--config=ubsan")
args+=("--deleted_packages=${deleted_packages:1}")
io::run bazel test "${args[@]}" --test_tag_filters=-integration-test "${BAZEL_TARGETS[@]}"

mapfile -t integration_args < <(integration::bazel_args)
integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"
