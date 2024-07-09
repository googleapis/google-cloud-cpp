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

export USE_BAZEL_VERSION=6.4.0

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
args+=(
  # For now, we continue to test Bazel 6.x without bzlmod. Once the minimum
  # supported version of Bazel is 7.x we can decide how to test without bzlmod.
  --noenable_bzlmod
  # Only run the unit tests, no need to waste time running everything.
  --test_tag_filters=-integration-test
)
bazel test "${args[@]}" -- "${BAZEL_TARGETS[@]}"
