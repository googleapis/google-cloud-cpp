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
source module ci/cloudbuild/builds/lib/cloudcxxrc.sh

export CC=clang
export CXX=clang++

mapfile -t args < <(bazel::common_args)
args+=("--//:enable_opentelemetry=false")
# Note that we do not ignore `//:opentelemetry`, as the exporters should be
# usable whether google-cloud-cpp is built with OpenTelemetry or not.
ignore=(
  # These integration tests use opentelemetry matchers out of convenience.
  "-//google/cloud/opentelemetry/integration_tests/..."
  # The quickstart demonstrates client tracing instrumentation, and thus it
  # depends on google-cloud-cpp being built with OpenTelemetry.
  "-//google/cloud/opentelemetry/quickstart/..."
)
bazel test "${args[@]}" --test_tag_filters=-integration-test -- "${BAZEL_TARGETS[@]}" "${ignore[@]}"
