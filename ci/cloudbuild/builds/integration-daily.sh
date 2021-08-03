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

# ATTENTION: This build is different than most because it's designed to be run
# as a "manual" trigger that's run on a schedule in GCB. It will be scheduled
# to run once a day. For details about scheduled manual triggers see
# https://cloud.google.com/build/docs/automating-builds/create-manual-triggers.
#
# NOTE: This build script will not have a trigger file in the
# `ci/cloudbuild/triggers` directory. "Manual" triggers exist only within the
# GCB UI at the time of this writing. Users with the appropriate access can run
# this build by hand with:
#   `ci/cloudbuild/build.sh --distro fedora-34 integration-daily --project cloud-cpp-testing-resources`

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/bazel.sh
source module ci/cloudbuild/builds/lib/integration.sh
source module ci/lib/io.sh

export CC=clang
export CXX=clang++

export GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_SAMPLES="yes"
mapfile -t args < <(bazel::common_args)
bazel test "${args[@]}" --test_tag_filters=-integration-test ...

export ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS="yes"
export GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS="instance,backup"
export GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_INTEGRATION_TESTS="yes"
mapfile -t integration_args < <(integration::bazel_args)
integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"

io::log_h2 "Running Spanner integration tests (against prod)"
bazel test "${args[@]}" "${integration_args[@]}" \
  --test_tag_filters="integration-test" google/cloud/spanner/...
