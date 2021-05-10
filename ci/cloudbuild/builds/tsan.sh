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

# Compile with the ThreadSanitizer enabled.
# We need to use clang-9 for the TSAN build because:
#   https://github.com/google/sanitizers/issues/1259
# Using Fedora >= 32 will push us to clang-10, and Fedora < 32 is EOL.
# Fortunately, Ubuntu:18.04 is a LTS release with clang-9 as an alternative.
export CC=clang-9
export CXX=clang++-9

mapfile -t args < <(bazel::common_args)
args+=("--config=tsan")
bazel test "${args[@]}" --test_tag_filters=-integration-test ...

mapfile -t integration_args < <(integration::bazel_args)
integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"
